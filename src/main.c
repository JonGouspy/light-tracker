#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "../Components/ili9341/ili9341.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "devices.h"
#include "main.h"

#define LUM_OFFSET      100


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

static void vDisplayTask(void *pvParameters);
static void vLumTask(void *pvParameters);

/* ******************************************************
 *      Global Context
 * ******************************************************/
int cycle = 0;
int nbTIM2 = 0;

uint16_t currLum = 0;
uint16_t sensor = 0;

int nbIDLE = 0;

DisplayInfo info = {
         .msg = {0},
         .prevTIM2 = 0,
         .TimFreq = 0,
         .toggled = false,
         .back_color = LCD_COLOR_LIGHTGRAY,
         .text_color = LCD_COLOR_BLACK
};

SemaphoreHandle_t xSemaphore;

/* ******************************************************
 *      Entry Point
 * ******************************************************/

int main(void)
{
    setup();                        // init devices

    xSemaphore = xSemaphoreCreateBinary();

    LCDdisplayInit(COLORS_NORMAL);  // init display

    TaskHandle_t xDisplayHandle = NULL;
    TaskHandle_t xLumHandle2 = NULL;

    xTaskCreate(vDisplayTask, "vDisplayTask", configMINIMAL_STACK_SIZE, &info, tskIDLE_PRIORITY + 1, &xDisplayHandle);
    xTaskCreate(vLumTask, "vLumTask", configMINIMAL_STACK_SIZE, &xSemaphore, tskIDLE_PRIORITY + 1, &xLumHandle2);

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

void ADC_IRQHandler(void)
{
    if (__HAL_ADC_GET_IT_SOURCE(&gAdcHandle, ADC_IT_EOC))
    {
        currLum = HAL_ADC_GetValue(&gAdcHandle);
        cycle++;
        __HAL_ADC_CLEAR_FLAG(&gAdcHandle, ADC_IT_EOC);
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
    initADC();
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
                xSemaphoreTake(xSemaphore, 5000);

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

                snprintf(data->msg, 20, "%d", cycle);                // ADC cycles
                LCDdisplayText(8, LCD_COLOR_GREEN, data->msg);

                snprintf(data->msg, 20, "%d", sensor);               // Sensor cycles
                LCDdisplayText(11, LCD_COLOR_RED, data->msg);

                // HAL_Delay(1000);                                     // Refresh = 1 second
                // vTaskDelay(1000);                                    // Enable in FreeRTOSConfig.H
                // vTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ); // Enable in FreeRTOSConfig.H
                data->TimFreq = nbTIM2 - data->prevTIM2;
                data->prevTIM2 = nbTIM2;

                BSP_LCD_ClearStringLine(12);
        }
}

static void vLumTask(void *pvParameters)
{
        SemaphoreHandle_t* xSemaphore = pvParameters;

        assert(xSemaphore != NULL);

        for( ;; )
        {
                if (currLum - LUM_OFFSET > sensor || currLum + LUM_OFFSET < sensor)
                {
                        sensor = currLum;

                        xSemaphoreGive(*xSemaphore);
                }
        }
}
