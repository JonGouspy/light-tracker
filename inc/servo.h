/*
 * servo.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

#ifndef SERVO_H_
#define SERVO_H_

/* Standard */
#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* Angle */
#define ANGLE_MIN_DEG               (-80)
#define ANGLE_MAX_DEG               (+80)

/* Pulse */
#define PULSE_CENTER_US             1000
#define PULSE_PER_90DEG_US          500

/* Timing */
#define SERVO_PERIOD_MS             20
#define TIMER_TICK_US               10

void vServoTask(void *pvParameters);

#endif /* SERVO_H_ */
