/*
 * display.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

/* Standard */
#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "queue.h"

/* Project */
#include "state.h"

/* Sensors and queue */
#define ADC_MAX                     4095
#define MQ_DISPLAY_LENGTH           4

/* Colors */
#define SCREEN_BACKGROUND           LCD_COLOR_WHITE
#define ANIM_BACKGROUND             LCD_COLOR_DARKBLUE

/* Eyes */
#define EYE_Y                       120
#define EYE_RADIUS                  50
#define PUPIL_RADIUS                18
#define PUPIL_TRAVEL_PX             (EYE_RADIUS - PUPIL_RADIUS - 4)

/* Light bars */
#define BAR_BOTTOM_Y                305
#define BAR_MAX_HEIGHT              100
#define BAR_WIDTH                   40

typedef struct {
    State state;
    int32_t left;
    int32_t right;
    int32_t diff;
    int32_t angle;
} DisplayMsg;

void vDisplayTask(void *unused);

#endif /* DISPLAY_H_ */
