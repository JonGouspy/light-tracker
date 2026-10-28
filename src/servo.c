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

#ifdef SERVO_TEST_SWEEP

#define SWEEP_RANGE_DEG             45
#define SWEEP_STEP_DEG              3

/**
 * @brief  Hardware test without the scheduler: sweeps the servo left and right forever.
 *         One control pulse per SERVO_PERIOD_MS (HAL_Delay), the timer interrupt lowers the signal.
 * @param  servo_ticks_up: pulse width in timer ticks, decremented by the timer interrupt
 * @retval never returns
 */
void Servo_TestSweep(volatile uint32_t *servo_ticks_up)
{
	int32_t angle = -SWEEP_RANGE_DEG;
	int32_t step = SWEEP_STEP_DEG;

	for (;;)
	{
		*servo_ticks_up = angleToPulseTicks(angle);
		BSP_LED_On(LED3);
		HAL_Delay(SERVO_PERIOD_MS);

		angle += step;
		if (angle >= SWEEP_RANGE_DEG || angle <= -SWEEP_RANGE_DEG)
			step = -step;
	}
}

#endif /* SERVO_TEST_SWEEP */

/**
 * @brief  Servo task, runs every SERVO_PERIOD_MS (vTaskDelayUntil).
 *         Writes the pulse width of the target angle, then raises the control
 *         signal (LED3 on PG13). The timer interrupt lowers it once the ticks are elapsed.
 * @param  pvParameters: pointer to the uint32_t holding the pulse width in timer ticks
 * @retval None
 */
void vServoTask(void *pvParameters)
{
	volatile uint32_t* servo_ticks_up = (volatile uint32_t*) pvParameters;

	assert(servo_ticks_up != NULL);

    TickType_t last_wake = xTaskGetTickCount();

	for (;;)
	{
        *servo_ticks_up = angleToPulseTicks(gAutomaton.target_angle);
        BSP_LED_On(LED3);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SERVO_PERIOD_MS));
	}
}
