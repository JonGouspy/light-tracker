#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "../Components/ili9341/ili9341.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "devices.h"
#include "main.h"

#define MAX (100000)


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
static void vPTask(void *pvParameters);

/* ******************************************************
 *      Global Context
 * ******************************************************/
int cycle = 0;
int nbTIM2 = 0;

DisplayInfo dispInfo = {
         .msg = {0},
         .prevTIM2 = 0,
         .TimFreq = 0,
         .toggled = false,
         .back_color = LCD_COLOR_LIGHTGRAY,
         .text_color = LCD_COLOR_BLACK
};

SemaphoreHandle_t xSemaphore;
SemaphoreHandle_t xMutex;
uint32_t cnt = 0;
uint32_t c1 = 0;
uint32_t c2 = 0;

/* ******************************************************
 *      Entry Point
 * ******************************************************/

int main(void)
{
    setup();                        // init devices

    xSemaphore = xSemaphoreCreateBinary();
    xMutex = xSemaphoreCreateMutex();

    LCDdisplayInit(COLORS_NORMAL);  // init display

    int displayPriority = tskIDLE_PRIORITY + 1;
    int pPriority = tskIDLE_PRIORITY + 2;
    TaskHandle_t xDisplayHandle = NULL;
    TaskHandle_t xP1Handle = NULL;
    TaskHandle_t xP2Handle = NULL;

    xTaskCreate(vDisplayTask, "vDisplayTask", configMINIMAL_STACK_SIZE, NULL, displayPriority, &xDisplayHandle);
    xTaskCreate(vPTask, "vP1Task", configMINIMAL_STACK_SIZE, &c1, pPriority, &xP1Handle);
    xTaskCreate(vPTask, "vP2Task", configMINIMAL_STACK_SIZE, &c2, pPriority, &xP2Handle);

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
    initTimer();
}

static inline void display()
{
        BSP_LCD_SetBackColor(dispInfo.back_color);

        snprintf(dispInfo.msg, 20, "CNT %ld", cnt);
        LCDdisplayText(5, dispInfo.text_color, dispInfo.msg);

        snprintf(dispInfo.msg, 20, "C1 %ld", c1);
        LCDdisplayText(8, LCD_COLOR_GREEN, dispInfo.msg);

        snprintf(dispInfo.msg, 20, "C2 %ld", c2);
        LCDdisplayText(11, LCD_COLOR_RED, dispInfo.msg);

        dispInfo.TimFreq = nbTIM2 - dispInfo.prevTIM2;
        dispInfo.prevTIM2 = nbTIM2;
}

/* ******************************************************
 *  Task Routines
 * ******************************************************/

static void vDisplayTask(void *pvParameters)
{
        assert(pvParameters == NULL);

        display();

        for( ;; )
        {
                if (BSP_PB_GetState(BUTTON_KEY) != 0)
                {
                        dispInfo.toggled = !dispInfo.toggled;
                        BSP_LED_Toggle(LED3);
                        LCDdisplayInit(dispInfo.toggled ? COLORS_REVERSE : COLORS_NORMAL);
                        dispInfo.back_color = dispInfo.toggled ? LCD_COLOR_DARKGRAY : LCD_COLOR_LIGHTGRAY;
                        dispInfo.text_color = dispInfo.toggled ? LCD_COLOR_WHITE : LCD_COLOR_BLACK;

                        display();
                }
        }
}

static void vPTask(void *pvParameters)
{
        uint32_t* task_cnt = pvParameters;

        assert(task_cnt != NULL);

        while (cnt < MAX)
        {
                xSemaphoreTake(xMutex, portMAX_DELAY);
                ++cnt;
                xSemaphoreGive(xMutex);

                ++(*task_cnt);
        }

        xSemaphoreTake(xSemaphore, portMAX_DELAY);

        for ( ;; );
}
