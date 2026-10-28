
#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "../Components/ili9341/ili9341.h"
#include "devices.h"
#include "main.h"

/* ******************************************************
 *      Global Context
 * ******************************************************/
int cycle = 0;
int nbTIM2 = 0;

#define SERVO_FRAME_TICKS       900
#define SERVO_PULSE_MIN_US      1100
#define SERVO_PULSE_MAX_US      1900
#define SERVO_PULSE_STEP_US     20

/* ******************************************************
 *      Entry Point
 * ******************************************************/

int main(void)
{
    /* Timer information */
    char msg[20] = {0};
    int prevTIM2 = 0;
    int TimFreq = 0;

    bool toggled = false;
    uint32_t back_color = LCD_COLOR_LIGHTGRAY;
    uint32_t text_color = LCD_COLOR_BLACK;

    setup();                        // init devices

    LCDdisplayInit(COLORS_NORMAL);  // init display

    for( ;; )
    {
        if (BSP_PB_GetState(BUTTON_KEY) != 0)
        {
            toggled = !toggled;
            LCDdisplayInit(toggled ? COLORS_REVERSE : COLORS_NORMAL);
            back_color = toggled ? LCD_COLOR_DARKGRAY : LCD_COLOR_LIGHTGRAY;
            text_color = toggled ? LCD_COLOR_WHITE : LCD_COLOR_BLACK;
        }

        BSP_LCD_SetBackColor(back_color);
        snprintf(msg, 20, "T2 %dHz", TimFreq);              // TIM2 frequence
        LCDdisplayText(5, text_color, msg);

        snprintf(msg, 20, "%d", nbTIM2);                     // TIM2 cycles
        LCDdisplayText(8, LCD_COLOR_GREEN, msg);

        HAL_Delay(1000);                                     // Refresh = 1 second
        TimFreq = nbTIM2 - prevTIM2;
        prevTIM2 = nbTIM2;
    }
}

/* ******************************************************
 *      Callbacks
 * ******************************************************/

/**
 * @brief  TIM2 interrupt (45 kHz): generates the servo signal on PG13 (LED3) and
 *         sweeps it left and right, without any task.
 *         A frame lasts SERVO_FRAME_TICKS (20 ms): the pin goes high at the first tick,
 *         low once the pulse width is elapsed. The pulse moves by SERVO_PULSE_STEP_US
 *         every frame between SERVO_PULSE_MIN_US and SERVO_PULSE_MAX_US.
 * @retval None
 */
void TIM2_IRQHandler(void)
{
    static uint32_t frame_tick = 0;
    static uint32_t pulse_ticks = 0;
    static int32_t pulse_us = SERVO_PULSE_MIN_US;
    static int32_t step_us = SERVO_PULSE_STEP_US;

    if (__HAL_TIM_GET_ITSTATUS(&gTimHandle, TIM_IT_UPDATE))
    {
        nbTIM2++;
        __HAL_TIM_CLEAR_IT(&gTimHandle, TIM_IT_UPDATE);

        if (frame_tick == 0)
        {
            pulse_ticks = pulse_us * 9 / 200;
            BSP_LED_On(LED3);

            pulse_us += step_us;
            if (pulse_us >= SERVO_PULSE_MAX_US || pulse_us <= SERVO_PULSE_MIN_US)
                step_us = -step_us;
        }
        else if (frame_tick == pulse_ticks)
        {
            BSP_LED_Off(LED3);
        }

        if (++frame_tick >= SERVO_FRAME_TICKS)
            frame_tick = 0;
    }
}

/* ******************************************************
 *  Functions
 * ******************************************************/

void setup(void)
{
    HAL_Init();
    SystemClock_Config();
    initLCD();
    initGPIO();
    initTimer();
}
