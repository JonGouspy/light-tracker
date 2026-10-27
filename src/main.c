
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
            BSP_LED_Toggle(LED3);
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

void TIM2_IRQHandler(void)
{
    if (__HAL_TIM_GET_ITSTATUS(&gTimHandle, TIM_IT_UPDATE))
    {
        nbTIM2++;
        __HAL_TIM_CLEAR_IT(&gTimHandle, TIM_IT_UPDATE);
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
