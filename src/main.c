#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "../Components/ili9341/ili9341.h"

#include "FreeRTOS.h"
#include "task.h"

#include "devices.h"
#include "main.h"


/* ******************************************************
 *      Local Declaration
 * ******************************************************/

typedef struct {
        char msg[20];
        int prevTIM2;
        int TimFreq;
        bool toggled;
        uint32_t back_color;
        uint32_t text_color;
} DisplayInfo;

static void vDisplay(void);

/* ******************************************************
 *      Global Context
 * ******************************************************/
int cycle = 0;
int nbTIM2 = 0;

int nbIDLE = 0;

DisplayInfo info = {
        .msg = {0},
        .prevTIM2 = 0,
        .TimFreq = 0,
        .toggled = false,
        .back_color = LCD_COLOR_LIGHTGRAY,
        .text_color = LCD_COLOR_BLACK
};

/* ******************************************************
 *      Entry Point
 * ******************************************************/

int main(void)
{
    setup();                        // init devices

    LCDdisplayInit(COLORS_NORMAL);  // init display

    vDisplay();

    vTaskStartScheduler();
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

/* ******************************************************
* IDLE
* ******************************************************/

void vApplicationIdleHook(void)
{
        nbIDLE++;
}

/* ******************************************************
* TICK
* ******************************************************/

void vApplicationTickHook(void)
{
        nbIDLE++;
}

/* ******************************************************
 *  Task Routines
 * ******************************************************/

static void vDisplayTask(void *pvParameters)
{
        DisplayInfo *data = pvParameters;

        assert(data != NULL);

        TickType_t xLastWakeTime;

        xLastWakeTime = xTaskGetTickCount();

        for( ;; )
        {
                if (BSP_PB_GetState(BUTTON_KEY) != 0)
                {
                        data->toggled = !data->toggled;
                        BSP_LED_Toggle(LED3);
                        LCDdisplayInit(data->toggled ? COLORS_REVERSE : COLORS_NORMAL);
                        data->back_color = data->toggled ? LCD_COLOR_DARKGRAY : LCD_COLOR_LIGHTGRAY;
                        data->text_color = data->toggled ? LCD_COLOR_WHITE : LCD_COLOR_BLACK;
                }

                BSP_LCD_SetBackColor(data->back_color);

                snprintf(data->msg, 20, "T2 %dHz", data->TimFreq);   // TIM2 frequence
                LCDdisplayText(5, data->text_color, data->msg);

                snprintf(data->msg, 20, "%d", nbTIM2);               // TIM2 cycles
                LCDdisplayText(8, LCD_COLOR_GREEN, data->msg);

                snprintf(data->msg, 20, "%d", nbIDLE);               // nbIDLE cycles
                LCDdisplayText(11, LCD_COLOR_RED, data->msg);

                // HAL_Delay(1000);                                  // Refresh = 1 second
                // vTaskDelay(1000);                                 // Enable in FreeRTOSConfig.H
                vTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ); // Enable in FreeRTOSConfig.H
                data->TimFreq = nbTIM2 - data->prevTIM2;
                data->prevTIM2 = nbTIM2;
        }
}

static void vDisplay(void)
{
        TaskHandle_t xHandle = NULL;

        /* Create the task, storing the handle. */
        xTaskCreate(vDisplayTask,                        /* Function that implements the task. */
                                "vDisplayTask",                  /* Text name for the task. */
                                configMINIMAL_STACK_SIZE,   /* Stack size in words, not bytes. */
                                &info,                                           /* Parameter passed into the task. */
                                tskIDLE_PRIORITY + 1,            /* Priority at which the task is created. */
                                &xHandle );                      /* Used to pass out the created task's handle. */
}
