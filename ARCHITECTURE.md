# Light Tracker – Architecture logicielle

Suiveur de lumière sur **STM32F429I-Discovery** (FreeRTOS + HAL).
Deux capteurs de lumière (ADC) pilotent un servomoteur pour l'orienter vers la source lumineuse, et l'écran LCD affiche l'état du système.

> Périmètre de ce document : `automaton`, `devices`, `display`, `event`, `main`, `state`, `utils` (`.c` et `.h`).
> `servo.c/h` est cité uniquement quand il est référencé par ces fichiers.

---

## 1. Vue d'ensemble

```
        ┌────────────────────────── MATÉRIEL ──────────────────────────┐
        │  Capteur gauche (PC1 / ADC1 ch.11)   Capteur droit (PA5 / ch.5)│
        │  Bouton USER                         Servo (PG13 / LED3)       │
        └───────┬───────────────────────────────────────▲───────────────┘
                │                                       │ signal PWM logiciel
        ┌───────▼────────────┐   interruptions    ┌─────┴──────────┐
        │  TIM2 (10 µs)      │───────────────────▶│ ADC (injecté)  │
        │  main.c            │  toutes les 10 ms  │ main.c         │
        └────────────────────┘                    └───────┬────────┘
                                                          │ Event (xQueueSendFromISR)
                                                  ┌───────▼────────┐
                                                  │   mq_event     │  8 éléments
                                                  └───────┬────────┘
                                                          │
 ┌───────────────┐   target_angle   ┌─────────────────────▼───────────┐
 │  vServoTask   │◀─────────────────│        vAutomatonTask           │
 │ (prio HAUTE)  │   (variable      │        (prio MOYENNE)           │
 │  servo.c      │    partagée)     │  automaton.c : machine à états  │
 └───────────────┘                  │  + suivi de la lumière          │
                                    └─────────────────────┬───────────┘
                                                          │ DisplayMsg (xQueueSend)
                                                  ┌───────▼────────┐
                                                  │  mq_display    │  4 éléments
                                                  └───────┬────────┘
                                                  ┌───────▼────────┐
                                                  │ vDisplayTask   │
                                                  │ (prio BASSE)   │
                                                  │ display.c → LCD│
                                                  └────────────────┘
```

Principe : les **interruptions** acquièrent les mesures et les transforment en **événements** ; une **tâche automate** décide de l'état et de l'angle cible ; une **tâche d'affichage** dessine ; une **tâche servo** convertit l'angle en impulsion. Les tâches ne communiquent que par files FreeRTOS et par la structure partagée `gAutomaton`.

---

## 2. Organisation des fichiers

| Fichier | Rôle |
|---|---|
| [`main.c`](src/main.c) / [`main.h`](inc/main.h) | Point d'entrée, création des files et tâches, ISR (TIM2, ADC), calibration, génération des événements |
| [`automaton.c`](src/automaton.c) / [`automaton.h`](inc/automaton.h) | Machine à états + algorithme de suivi, structure partagée `gAutomaton` |
| [`state.h`](inc/state.h) | Énumération `State` |
| [`event.h`](inc/event.h) | Type `Event` et `EventType` (capteurs → automate) |
| [`display.c`](src/display.c) / [`display.h`](inc/display.h) | Tâche d'affichage et trois écrans (STOP, animation, debug), type `DisplayMsg` |
| [`devices.c`](src/devices.c) / [`devices.h`](inc/devices.h) | Initialisation matérielle : horloge, GPIO, ADC, timer, LCD ; bas niveau LCD |
| [`utils.c`](src/utils.c) / [`utils.h`](inc/utils.h) | `abs` et `clamp` |

### Dépendances entre modules

```
main ──▶ automaton ──▶ event, state, utils, servo
  │         ▲
  │         └──────────── display ──▶ state, automaton, devices, utils, servo
  └──▶ devices, display, event, utils, servo
```

`state.h`, `event.h` et `utils.h` sont les briques de base : elles ne dépendent d'aucun autre module du projet.

---

## 3. Types de données partagés

### `State` – [state.h](inc/state.h)
```c
STATE_STOP, STATE_TRACK_ANIM, STATE_TRACK_DEBUG, STATE_NONE
```
`STATE_NONE` n'est pas un état réel : il sert de valeur initiale à la tâche d'affichage pour forcer un premier dessin.

### `Event` – [event.h](inc/event.h)
```c
typedef struct { EventType type; int32_t left, right, diff; } Event;
// EVT_BUTTON : appui sur le bouton USER
// EVT_LIGHT  : variation de lumière significative
```
Produit par l'ISR ADC, consommé par l'automate. `diff` = `left - right - sensor_offset` (écart corrigé de la calibration).

### `DisplayMsg` – [display.h](inc/display.h)
```c
typedef struct { State state; int32_t left, right, diff, angle; } DisplayMsg;
```
Produit par l'automate, consommé par l'affichage : tout ce qu'il faut pour dessiner un écran.

### `Automaton` (`gAutomaton`) – [automaton.h](inc/automaton.h)
```c
typedef struct {
    QueueHandle_t mq_display;      // automate → affichage
    QueueHandle_t mq_event;        // ISR ADC → automate
    volatile int32_t target_angle; // automate → tâche servo
} Automaton;
```
Seul objet global partagé entre les tâches : il regroupe les deux files et l'angle cible. Il est initialisé dans `main()`.

---

## 4. `main.c` – démarrage, interruptions, acquisition

### 4.1 Séquence de démarrage (`main`)
1. `setup()` : `HAL_Init` → `SystemClock_Config` → `initLCD` → `initGPIO` → `initTimer` → `initADC`.
2. `LCDdisplayInit(COLORS_NORMAL)` : titre et cadre initiaux.
3. Création des files : `mq_display` (4 × `DisplayMsg`), `mq_event` (8 × `Event`), `target_angle = 0`.
4. Création des tâches, puis `vTaskStartScheduler()`.

| Tâche | Pile (mots) | Priorité | Période / déclenchement |
|---|---|---|---|
| `vServoTask` (servo.c) | 128 | `PRIO_HIGH` (idle+3) | 20 ms (`vTaskDelayUntil`) |
| `vAutomatonTask` | 512 | `PRIO_MID` (idle+2) | événement, timeout 20 ms |
| `vDisplayTask` | 512 | `PRIO_LOW` (idle+1) | bloquée sur `mq_display` |

La priorité la plus haute va au servo pour que la période de 20 ms soit respectée même pendant un redessin de l'écran.

Option de test matériel : `SERVO_TEST_SWEEP` (désactivée par défaut dans `servo.h`) fait balayer le servo avant le démarrage du scheduler.

### 4.2 Chaîne d'acquisition (interruptions)

1. **`TIM2_IRQHandler`** (priorité NVIC 0), toutes les `TIMER_TICK_US` = 10 µs :
   - décrémente `servo_ticks_up` ; à zéro, éteint LED3 (PG13) = **fin de l'impulsion servo** ;
   - tous les `ADC_PERIOD_TICKS` = 1000 ticks (**10 ms**), lance une conversion ADC injectée.
2. **`ADC_IRQHandler`** (priorité NVIC 5) → `HAL_ADC_IRQHandler`.
3. **`HAL_ADCEx_InjectedConvCpltCallback`** : fin des 2 conversions (gauche, droite).
   - **Calibration** : les `CALIBRATION_SAMPLES` = 16 premières mesures calculent `sensor_offset` (écart moyen quand les deux capteurs reçoivent la même lumière). Aucun événement n'est émis pendant ce temps.
   - Ensuite, `diff = left - right - sensor_offset`.
   - **`EVT_BUTTON`** envoyé sur front montant du bouton USER (lu en polling, anti-répétition par `button_was_pressed`).
   - **`EVT_LIGHT`** envoyé si l'écart a varié de plus de `LIGHT_CHANGE_THRESHOLD` depuis le dernier envoi, **ou** si `|diff| > TRACKING_DEAD_ZONE` (pour continuer à corriger tant que l'écart persiste).
   - Envois via `xQueueSendFromISR` + `portYIELD_FROM_ISR`.

`HAL_SYSTICK_Callback` n'appelle `xPortSysTickHandler` qu'une fois le scheduler démarré (le HAL utilise SysTick avant).

### 4.3 Génération du signal servo
Le servo est piloté par un **PWM logiciel** : la tâche servo écrit la largeur d'impulsion (en ticks de 10 µs) dans `servo_ticks_up`, et TIM2 abaisse la ligne quand le compteur arrive à zéro. Le signal est sur **PG13, partagé avec LED3**.

---

## 5. `automaton.c` – cerveau du système

### 5.1 Machine à états (`nextState`)

```
                bouton  ou  |diff| > START_THRESHOLD
   ┌──────────┐ ─────────────────────────────────▶ ┌────────────────┐
   │   STOP   │                                    │  TRACK_ANIM    │◀──┐
   └──────────┘                                    └───────┬────────┘   │
                                                      bouton│            │ bouton
                                                   ┌───────▼────────┐   │
                                                   │  TRACK_DEBUG   │───┘
                                                   └────────────────┘
```

- On ne **revient jamais** à `STOP` : après le démarrage, le bouton alterne entre animation et debug.
- Seul `STOP → ANIM` peut aussi être déclenché par la lumière (`START_THRESHOLD` = 1000).

### 5.2 Suivi de la lumière (`trackLight`)
Appelé à chaque événement reçu, hors `STOP` :
- **Zone morte** : si `|diff| ≤ TRACKING_DEAD_ZONE` (150) → rien.
- **Pas proportionnel** : `step = TRACKING_DIRECTION * diff / TRACKING_GAIN_DIVISOR` (150) ; au minimum ±1 si la division donne 0.
- **Limite de vitesse** : `step` borné à ±`TRACKING_MAX_STEP_DEG` (4°).
- **Limite de course** : `target_angle` borné à `[ANGLE_MIN_DEG, ANGLE_MAX_DEG]` = [-80°, +80°].
- `TRACKING_DIRECTION` (+1 / -1) permet d'inverser le sens selon le montage.

### 5.3 Boucle de la tâche (`vAutomatonTask`)
```
publishDisplay(STOP, {0})                      // écran initial
loop:
    event = attendre mq_event (max SERVO_PERIOD_MS = 20 ms)
    si reçu : latest = event ; state = nextState(state, event)
    si reçu et state != STOP : trackLight(latest.diff)
    si reçu ou angle modifié : publishDisplay(state, latest)
```
- Le timeout de 20 ms garde la boucle vivante même sans événement.
- `publishDisplay` utilise `xQueueSend(..., 0)` : **non bloquant**, le message est abandonné si la file est pleine. L'automate n'est donc jamais ralenti par l'écran.

---

## 6. `display.c` – tâche d'affichage

`vDisplayTask` bloque sur `mq_display` (`portMAX_DELAY`). À chaque message :
- `has_changed = (msg.state != état précédent)` ;
- selon l'état, appelle `drawStop` / `drawAnimation` / `drawDebug`.

Principe d'optimisation commun : **redessin complet uniquement au changement d'état**, ensuite on ne redessine que ce qui a bougé (variables `static prev_*`, `snapToStep` pour limiter les redessins).

| État | Écran | Contenu |
|---|---|---|
| `STATE_STOP` | `drawStop` (une seule fois) | Titre, symbole stop, consignes (« Bouton USER » ou lumière) |
| `STATE_TRACK_ANIM` | `drawAnimation` | Ciel dont la teinte suit la luminosité (6 niveaux `SKY_LEVELS`), soleil/lune qui se déplace selon l'angle, tournesol qui penche, pourcentage de lumière |
| `STATE_TRACK_DEBUG` | `drawDebug` | Deux jauges à aiguille (gauche/droite), barre centrée de l'écart, curseur d'angle servo, largeur d'impulsion en µs |

Helpers de dessin : `sinDeg`/`cosDeg` (table de sinus entière × 1000, sans flottant), `drawText`, `drawTextCentered`, `clearScreen`, `drawNeedle`, `drawGaugeFace`, `updateGauge`, `drawSun`, `drawFlower`.

Le fichier `display.h` regroupe aussi les constantes d'affichage (couleurs `DBG_*`, géométrie des jauges, du soleil et de la fleur) et `ADC_MAX` (4095).

---

## 7. `devices.c` – matériel

| Fonction | Rôle |
|---|---|
| `SystemClock_Config` | HSE 8 MHz → PLL → SYSCLK 180 MHz (Over-Drive), APB1 /4, APB2 /2 |
| `initGPIO` | Bouton USER (`BSP_PB_Init`) + `Servo_Init` (PG13 en sortie push-pull, niveau bas) |
| `initADC` | ADC1, 12 bits, **2 canaux injectés** (ch.11 = PC1 gauche, ch.5 = PA5 droite), démarrage logiciel, 480 cycles d'échantillonnage, IT activée, pas de DMA |
| `initTimer` | TIM2, période 900 ticks à 90 MHz = **10 µs**, interruption de mise à jour, priorité NVIC 0 |
| `initLCD` | LCD + 2 couches (background/foreground), couche foreground sélectionnée |
| `LCDdisplayInit` | Titre « -GISTRE27- » et cadre (version normale/inversée) *(déclaré dans `main.h`)* |
| `LCDdisplayText` | Texte centré sur une ligne |
| `HAL_LTDC_LineEvenCallback` | Callback LTDC (repositionnement de couche, reprogrammation de l'événement de ligne) |

Les handles `gTimHandle` et `gAdcHandle` sont définis dans `devices.c` (via `DEFINE_GLOBALS`) et exposés en `extern` par `devices.h`.

---

## 8. `utils.c` – utilitaires

- `abs(int32_t)` : valeur absolue.
- `clamp(value, min, max)` : bornage.

Utilisés par l'automate (suivi, seuils), l'affichage (normalisation des mesures) et l'ISR ADC (variation de lumière).

---

## 9. Paramètres de réglage

| Constante | Valeur | Fichier | Effet |
|---|---|---|---|
| `START_THRESHOLD` | 1000 | automaton.h | Écart de lumière qui fait sortir de `STOP` |
| `LIGHT_CHANGE_THRESHOLD` | 100 | automaton.h | Variation minimale pour émettre `EVT_LIGHT` |
| `TRACKING_DEAD_ZONE` | 150 | automaton.h | Écart ignoré par le suivi |
| `TRACKING_GAIN_DIVISOR` | 150 | automaton.h | Plus grand = suivi plus doux |
| `TRACKING_MAX_STEP_DEG` | 4 | automaton.h | Vitesse maximale (°/événement) |
| `TRACKING_DIRECTION` | +1 | automaton.h | Sens de rotation (à inverser si le servo va à l'envers) |
| `MQ_EVENT_LENGTH` / `MQ_DISPLAY_LENGTH` | 8 / 4 | automaton.h / display.h | Taille des files |
| `ADC_PERIOD_TICKS` | 1000 | main.c | Mesure toutes les 10 ms |
| `CALIBRATION_SAMPLES` | 16 | main.c | Nombre de mesures de calibration |

---

## 10. Points d'attention

- **`utils.c` redéfinit `abs`**, déjà fourni par `<stdlib.h>` (signature `int`) : risque de conflit ou d'avertissement selon les inclusions.
- `drawAnimation` et `drawDebug` ne sont pas `static` alors qu'elles ne sont utilisées que dans `display.c`.
- Le commentaire d'`initTimer` annonce « 100 kHz, 10 µs » : le calcul réel est 90 MHz / 900 = 100 kHz, soit bien 10 µs, mais la mention « prescaler 0 » avec « tick 100 kHz » prête à confusion (le tick du timer est à 90 MHz, c'est la période de 900 qui donne les 10 µs).
- Les `static prev_*` de `display.c` ne sont pas réinitialisés hors du flag `clear` : le design suppose qu'un seul fil d'exécution (la tâche d'affichage) dessine.
