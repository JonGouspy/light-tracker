/*
 * servo.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

/* Standard */
#include <assert.h>

/* Board */
#include "stm32f429i_discovery.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* Project */
#include "automaton.h"
#include "devices.h"
#include "servo.h"
#include "utils.h"

/**
 * @brief  Converts an angle into the high level duration of the control signal.
 * @param  angle_deg: angle in degrees, limited to ANGLE_MIN_DEG..ANGLE_MAX_DEG
 * @retval pulse width in timer ticks (TIMER_TICK_US each)
 */
static uint32_t angleToPulseTicks(int32_t angle_deg)
{
    angle_deg = clamp(angle_deg, ANGLE_MIN_DEG, ANGLE_MAX_DEG);
    int32_t pulse_us = PULSE_CENTER_US + angle_deg * PULSE_PER_90DEG_US / 90;
    return pulse_us / TIMER_TICK_US;
}

/**
 * @brief  Servo task, runs every SERVO_PERIOD_MS (vTaskDelayUntil).
 *         Writes the pulse width of the target angle, then raises the control
 *         signal (LED3 on PG13). The timer interrupt lowers it once the ticks are elapsed.
 * @param  pvParameters: pointer to the uint32_t holding the pulse width in timer ticks
 * @retval None
 */
void vServoTask(void *pvParameters)
{
	uint32_t* servo_ticks_up = (uint32_t*) pvParameters;

	assert(servo_ticks_up != NULL);

    TickType_t last_wake = xTaskGetTickCount();

	for (;;)
	{
        *servo_ticks_up = angleToPulseTicks(gAutomaton.target_angle);
        BSP_LED_On(LED3);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SERVO_PERIOD_MS));
	}
}
