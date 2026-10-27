/*
 * main.c
 *
 */

/* Board */
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Project */
#include "automaton.h"
#include "devices.h"
#include "display.h"
#include "event.h"
#include "main.h"
#include "servo.h"

/* ******************************************************
 *      Settings
 * ******************************************************/

#define PRIO_HIGH   (tskIDLE_PRIORITY + 3)
#define PRIO_MID    (tskIDLE_PRIORITY + 2)
#define PRIO_LOW    (tskIDLE_PRIORITY + 1)

/* ******************************************************
 *      Global Context
 * ******************************************************/

int cycle = 0;
int nbTIM2 = 0;

/* Pulse width in timer ticks: written by Servo, decremented by TIM2 */
static uint32_t servo_ticks_up = 0;

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
 * @brief  TIM2 interrupt: counts the update events.
 * @retval None
 */
void TIM2_IRQHandler(void)
{
    if (__HAL_TIM_GET_ITSTATUS(&gTimHandle, TIM_IT_UPDATE))
    {
        nbTIM2++;
        __HAL_TIM_CLEAR_IT(&gTimHandle, TIM_IT_UPDATE);
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
 * @brief  TIM7 interrupt: generates the servo control signal.
 * @retval None
 */
void TIM7_IRQHandler(void)
{
    if (__HAL_TIM_GET_ITSTATUS(&gServoTimHandle, TIM_IT_UPDATE))
    {
        __HAL_TIM_CLEAR_IT(&gServoTimHandle, TIM_IT_UPDATE);
        Servo_TimerUpdate();
    }
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
    initServoTimer();
    initADC();
}
