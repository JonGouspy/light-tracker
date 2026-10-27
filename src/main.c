#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "../Components/ili9341/ili9341.h"
#include "devices.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


/* ******************************************************
 *      Prototypes
 * ******************************************************/

void DisplayMsgScroll(char* msg);

/* ******************************************************
 *      Global Context
 * ******************************************************/
int cycle = 0;
int nbIdle = 0;
int nbTicks = 0;
int currentLine = 3;

/*
  * USE_MUTEX == 1 => sync is a mutex
  * else sync is a binary semaphore
  */
#define USE_MUTEX 1

SemaphoreHandle_t sync;

TaskHandle_t HighT;
TaskHandle_t MiddleT;
TaskHandle_t LowT;

static volatile double global = 10;

void lowTask(void* arg)
{
        DisplayMsgScroll("LOW-takes");

        // -TODO-  TAKE SEMAPHORE
        xSemaphoreTake(sync, portMAX_DELAY);
        //

        xTaskNotifyGive(HighT);  // free HIGH
        xTaskNotifyGive(MiddleT);// free MIDDLE

        DisplayMsgScroll("LOW-release");  // allowed to run (again...)

        // -TODO- RELEASE SEMAPHORE
        xSemaphoreGive(sync);
        //

        for(;;);
        {
         vTaskDelay(1000);
        }
}

void middleTask(void* arg)
{
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(8000)); // WAIT SYNC FROM LOW
        DisplayMsgScroll("MIDDLE-on");

        // -TODO-  100% ACTIVITY
        HAL_Delay(2000);
        //

        DisplayMsgScroll("MIDDLE-off");

        for(;;)
        {
        	vTaskDelay(1000);
        }
}

void highTask(void* arg)
{
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(8000));// WAIT SYNC FROM LOW
        vTaskDelay(pdMS_TO_TICKS(100));              // YIELD LOW (to sync MIDDLE)
        DisplayMsgScroll("HIGH-ask");

        // -TODO- TAKE SEMAPHORE
        xSemaphoreTake(sync, portMAX_DELAY);
        //

        DisplayMsgScroll("HIGH-has");

        for(;;)
        {
        	vTaskDelay(1000);
        }
}

int main(void)
{
    setup();
    LCDdisplayInit(COLORS_NORMAL);  // init display

    // Semaphore / Mutex
#ifdef USE_MUTEX
    sync = xSemaphoreCreateMutex();
#else
    sync = xSemaphoreCreateBinary();
    xSemaphoreGive(sync);
#endif

    xTaskCreate(lowTask,
        "Low",
        512,
        (void*)1,
        configMAX_PRIORITIES - 3,
        &LowT);

    xTaskCreate(middleTask,
        "Middle",
        512,
        (void*)2,
        configMAX_PRIORITIES - 2,
        &MiddleT);

    xTaskCreate(highTask,
        "High",
        512,
        (void*)3,
        configMAX_PRIORITIES - 1,
        &HighT);


    /* Start the scheduler */
    vTaskStartScheduler();

    return 0;
}

/* ******************************************************
 *  FreeRTOS
 * ******************************************************/
void vApplicationIdleHook()
{
    nbIdle++;
}

void vApplicationTickHook()
{
    nbTicks++;
}

void DisplayMsgScroll(char* msg)
{
    uint32_t back_color = LCD_COLOR_LIGHTGRAY;
    uint32_t text_color = LCD_COLOR_BLACK;

    BSP_LCD_SetBackColor(back_color);
    LCDdisplayText(currentLine, text_color, msg);
    currentLine++;
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
}
