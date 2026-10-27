/*
 * display.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

/* Standard */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

/* Board */
#include "stm32f429i_discovery_lcd.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "queue.h"

/* Project */
#include "automaton.h"
#include "devices.h"
#include "display.h"
#include "utils.h"

static const uint16_t EYE_X[2] = { 65, 175 };
static const uint16_t BAR_X[2] = { 45, 155 };

/**
 * @brief  Fills the whole screen and uses the same color as text background.
 * @param  background: LCD color
 * @retval None
 */
static void clearScreen(uint32_t background)
{
    BSP_LCD_Clear(background);
    BSP_LCD_SetBackColor(background);
}

/**
 * @brief  Draws the STOP screen with the instructions to start.
 * @retval None
 */
static void drawStop(void)
{
    clearScreen(SCREEN_BACKGROUND);
    LCDdisplayText(5, LCD_COLOR_RED,   "STOP");
    LCDdisplayText(8, LCD_COLOR_BLACK, "Button ou light");
    LCDdisplayText(9, LCD_COLOR_BLACK, "to start");
}

/**
 * @brief  Draws the two eyes, the pupils follow the servo angle.
 *         Without redraw_all, only the pupils move and nothing is drawn if they did not.
 * @param  angle: servo angle in degrees
 * @param  redraw_all: true to draw the whole eyes, false to move the pupils only
 * @retval None
 */
static void drawEyes(int32_t angle, bool redraw_all)
{
    static int32_t previous_offset = 0;
    int32_t offset = angle * PUPIL_TRAVEL_PX / 90;

    if (!redraw_all && offset == previous_offset) return;

    for (int i = 0; i < 2; i++) {
        BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
        if (redraw_all)
            BSP_LCD_FillCircle(EYE_X[i], EYE_Y, EYE_RADIUS);
        else
            BSP_LCD_FillCircle(EYE_X[i] + previous_offset, EYE_Y, PUPIL_RADIUS);

        BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
        BSP_LCD_DrawCircle(EYE_X[i], EYE_Y, EYE_RADIUS);
        BSP_LCD_FillCircle(EYE_X[i] + offset, EYE_Y, PUPIL_RADIUS);
    }
    previous_offset = offset;
}

/**
 * @brief  Draws a vertical bar whose height is proportional to the light level.
 * @param  x: left position of the bar
 * @param  level: sensor value, from 0 to ADC_MAX
 * @retval None
 */
static void drawLightBar(uint16_t x, int32_t level)
{
    uint16_t height = clamp(level, 0, ADC_MAX) * BAR_MAX_HEIGHT / ADC_MAX;

    BSP_LCD_SetTextColor(ANIM_BACKGROUND);
    BSP_LCD_FillRect(x, BAR_BOTTOM_Y - BAR_MAX_HEIGHT, BAR_WIDTH, BAR_MAX_HEIGHT);
    if (height > 0) {
        BSP_LCD_SetTextColor(LCD_COLOR_YELLOW);
        BSP_LCD_FillRect(x, BAR_BOTTOM_Y - height, BAR_WIDTH, height);
    }
}

/**
 * @brief  Draws the animation mode: eyes following the light and one bar per sensor.
 * @param  msg: state, measures and angle to display
 * @param  clear: true on entering the mode, to redraw the whole screen
 * @retval None
 */
void drawAnimation(DisplayMsg *msg, bool clear)
{
	if (clear) {
		clearScreen(SCREEN_BACKGROUND);
		LCDdisplayText(0, LCD_COLOR_BLUE, "ANIM MODE");
	}

	drawEyes(msg->angle, clear);
	drawLightBar(BAR_X[0], msg->left);
	drawLightBar(BAR_X[1], msg->right);
}

/**
 * @brief  Draws the debug mode: measures of both sensors, their gap and the servo angle.
 * @param  msg: state, measures and angle to display
 * @param  clear: true on entering the mode, to redraw the whole screen
 * @retval None
 */
void drawDebug(DisplayMsg *msg, bool clear)
{
	char line[24];

	if (clear) {
		clearScreen(SCREEN_BACKGROUND);
		LCDdisplayText(0, LCD_COLOR_BLUE, "DEBUG MODE");
	}

	snprintf(line, sizeof line, "Left : %5ld", (long)msg->left);
	LCDdisplayText(2, LCD_COLOR_BLACK, line);
	snprintf(line, sizeof line, "Right : %5ld", (long)msg->right);
	LCDdisplayText(3, LCD_COLOR_BLACK, line);
	snprintf(line, sizeof line, "Gap  : %5ld", (long)msg->diff);
	LCDdisplayText(4, LCD_COLOR_BLACK, line);
	snprintf(line, sizeof line, "Angle  : %5ld°", (long)msg->angle);
	LCDdisplayText(5, LCD_COLOR_BLACK, line);
}

/**
 * @brief  Display task. Waits for a message on gAutomaton.mq_display and draws
 *         the screen of the received state, redrawing everything when the state changes.
 * @param  unused: not used, the queue is shared through gAutomaton
 * @retval None
 */
void vDisplayTask(void *unused)
{
	assert(gAutomaton.mq_display != NULL);

	State st = STATE_NONE;
	DisplayMsg msg;

	for( ;; )
	{
		xQueueReceive(gAutomaton.mq_display, &msg, portMAX_DELAY);
		bool has_changed = msg.state != st;
		st = msg.state;

		switch (st)
		{
		case STATE_STOP:
			if (has_changed)
				drawStop();
			break;
		case STATE_TRACK_ANIM:
			drawAnimation(&msg, has_changed);
			break;
		case STATE_TRACK_DEBUG:
			drawDebug(&msg, has_changed);
			break;
		default:
			break;
		}
	}
}
