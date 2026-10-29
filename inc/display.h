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

/* Debug screen */
#define DBG_BACKGROUND              0xFF0D1B2A
#define DBG_PANEL                   0xFF1B2B3F
#define DBG_ACCENT                  0xFF00E5FF
#define DBG_TEXT                    0xFFE0E6ED
#define DBG_DIM                     0xFF7A8CA3
#define DBG_LEFT                    0xFF00B4D8
#define DBG_RIGHT                   0xFFFF8A3D
#define GAUGE_CY                    120
#define GAUGE_RADIUS                48
#define GAUGE_NEEDLE_LEN            36

/* Animation screen: sky, sun and sunflower */
#define SKY_LEVELS                  6
#define SKY_HEIGHT                  240
#define SUN_Y                       50
#define SUN_TRAVEL_PX               85
#define FLOWER_X                    120
#define FLOWER_Y                    175
#define FLOWER_TRAVEL_PX            55
#define FLOWER_STEP_PX              4

typedef struct {
    State state;
    int32_t left;
    int32_t right;
    int32_t diff;
    int32_t angle;
} DisplayMsg;

void vDisplayTask(void *unused);

#endif /* DISPLAY_H_ */
