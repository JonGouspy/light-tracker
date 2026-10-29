/*
 * automaton.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

#ifndef AUTOMATON_H_
#define AUTOMATON_H_

/* Standard */
#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "queue.h"

/* Light thresholds */
#define START_THRESHOLD             1000
#define LIGHT_CHANGE_THRESHOLD      100

/* Tracking */
#define TRACKING_DEAD_ZONE          150
#define TRACKING_GAIN_DIVISOR       150
#define TRACKING_MAX_STEP_DEG       4
#define TRACKING_DIRECTION          (+1)

/* Event queue */
#define MQ_EVENT_LENGTH             8

typedef struct {
    QueueHandle_t mq_display;
    QueueHandle_t mq_event;
    volatile int32_t target_angle;
} Automaton;

extern Automaton gAutomaton;

void vAutomatonTask(void *unused);

#endif /* AUTOMATON_H_ */
