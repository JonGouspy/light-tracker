/*
 * main.c
 *
 */

/* Standard */
#include <stdbool.h>

/* Board */
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

extern void xPortSysTickHandler(void);

/* Project */
#include "automaton.h"
#include "devices.h"
#include "display.h"
#include "event.h"
#include "main.h"
#include "servo.h"
#include "utils.h"

/* ******************************************************
 *      Settings
 * ******************************************************/

#define PRIO_HIGH   (tskIDLE_PRIORITY + 3)
#define PRIO_MID    (tskIDLE_PRIORITY + 2)
#define PRIO_LOW    (tskIDLE_PRIORITY + 1)

#define ADC_PERIOD_TICKS    1000
#define CALIBRATION_SAMPLES 16

/* ******************************************************
 *      Global Context
 * ******************************************************/

int cycle = 0;
int nbTIM2 = 0;

/* Pulse width in timer ticks: written by Servo, decremented by TIM2 */
static volatile uint32_t servo_ticks_up = 0;

/* Gap measured with the same light on both sensors, subtracted from every diff */
static volatile int32_t sensor_offset = 0;
static volatile bool calibrated = false;

/* ******************************************************
 *      Entry Point
 * ******************************************************/

/**
 * @brief  Initializes the devices and the display, creates the queues and the
 *         tasks (Servo, Automaton, Display), then starts the scheduler.
 * @retval never returns
 */
int main(void)
{
    setup();
    LCDdisplayInit(COLORS_NORMAL);

#ifdef SERVO_TEST_SWEEP
    Servo_TestSweep(&servo_ticks_up);
#endif

    gAutomaton.mq_display = xQueueCreate(MQ_DISPLAY_LENGTH, sizeof(DisplayMsg));
    gAutomaton.mq_event = xQueueCreate(MQ_EVENT_LENGTH, sizeof(Event));
    gAutomaton.target_angle = 0;

    xTaskCreate(vServoTask, "Servo", 128, &servo_ticks_up, PRIO_HIGH, NULL);
    xTaskCreate(vAutomatonTask, "Automaton", 512, NULL, PRIO_MID, NULL);
    xTaskCreate(vDisplayTask, "Display", 512, NULL, PRIO_LOW, NULL);

    vTaskStartScheduler();

    return 0;
}

/* ******************************************************
 *      Callbacks
 * ******************************************************/

/**
 * @brief  SysTick callback: gives the FreeRTOS tick once the scheduler is running.
 * @retval None
 */
void HAL_SYSTICK_Callback(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
}

/**
 * @brief  TIM2 interrupt, every TIMER_TICK_US: lowers the servo signal once the
 *         pulse width is elapsed and starts an ADC measurement every ADC_PERIOD_TICKS.
 * @retval None
 */
void TIM2_IRQHandler(void)
{
    static uint32_t ticks_since_adc = 0;

    TIM2->SR = ~TIM_SR_UIF;

    if (servo_ticks_up > 0 && --servo_ticks_up == 0)
        BSP_LED_Off(LED3);

    if (++ticks_since_adc >= ADC_PERIOD_TICKS)
    {
        ticks_since_adc = 0;
        HAL_ADCEx_InjectedStart_IT(&gAdcHandle);
    }
}

/**
 * @brief  ADC interrupt: lets the HAL handle the end of conversion.
 * @retval None
 */
void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&gAdcHandle);
}

/**
 * @brief  End of the injected conversions (left, right).
 *         The first CALIBRATION_SAMPLES measures give sensor_offset (no event sent).
 *         Afterwards sends EVT_BUTTON on a press and EVT_LIGHT when the gap
 *         moved by more than LIGHT_CHANGE_THRESHOLD.
 * @param  hadc: ADC handle
 * @retval None
 */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    static int32_t last_sent_diff = 0;
    static bool button_was_pressed = false;
    static int32_t calibration_sum = 0;
    static int32_t calibration_count = 0;
    BaseType_t higher_priority_woken = pdFALSE;
    Event evt;

    evt.left = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
    evt.right = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);

    if (!calibrated)
    {
        calibration_sum += evt.left - evt.right;
        if (++calibration_count >= CALIBRATION_SAMPLES)
        {
            sensor_offset = calibration_sum / CALIBRATION_SAMPLES;
            calibrated = true;
        }
        return;
    }

    if (gAutomaton.mq_event == NULL)
        return;

    evt.diff = evt.left - evt.right - sensor_offset;

    bool button_pressed = BSP_PB_GetState(BUTTON_KEY) == GPIO_PIN_SET;
    if (button_pressed && !button_was_pressed)
    {
        evt.type = EVT_BUTTON;
        xQueueSendFromISR(gAutomaton.mq_event, &evt, &higher_priority_woken);
    }
    button_was_pressed = button_pressed;

    if (abs(evt.diff - last_sent_diff) > LIGHT_CHANGE_THRESHOLD)
    {
        evt.type = EVT_LIGHT;
        xQueueSendFromISR(gAutomaton.mq_event, &evt, &higher_priority_woken);
        last_sent_diff = evt.diff;
    }

    portYIELD_FROM_ISR(higher_priority_woken);
}

/* ******************************************************
 *      Functions
 * ******************************************************/

/**
 * @brief  Initializes the HAL, the system clock and every device.
 * @retval None
 */
void setup(void)
{
    HAL_Init();
    SystemClock_Config();
    initLCD();
    initGPIO();
    initTimer();
    initADC();
}
