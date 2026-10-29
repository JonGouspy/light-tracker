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
#include <string.h>

/* Board */
#include "stm32f429i_discovery_lcd.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "queue.h"

/* Project */
#include "automaton.h"
#include "devices.h"
#include "display.h"
#include "servo.h"
#include "utils.h"

#define SCREEN_WIDTH            240
#define SCREEN_CENTER_X         (SCREEN_WIDTH / 2)
#define GROUND_COLOR            0xFF3A7D2C
#define GROUND_EDGE_COLOR       0xFF2E6322
#define STEM_COLOR              0xFF2E7D32
#define LEAF_COLOR              0xFF4CAF50
#define PETAL_COLOR             0xFFFFC107
#define FLOWER_CORE_COLOR       0xFF5D3A1A
#define SUN_RAY_MIN             3
#define SUN_RAY_MAX             9
#define SUN_ERASE_HALF          33
#define PETAL_DISTANCE          30
#define PETAL_RADIUS            13
#define FLOWER_CORE_RADIUS      20
#define BAR_HALF_WIDTH          105

/* sin(0..180 deg) x 1000, every 10 degrees */
static const int16_t SIN_TABLE[19] = {
    0, 174, 342, 500, 643, 766, 866, 940, 985, 1000,
    985, 940, 866, 766, 643, 500, 342, 174, 0
};

/* Eight directions (cos, sin) x 1000, every 45 degrees */
static const int16_t RAY_DIR[8][2] = {
    { 1000, 0 }, { 707, 707 }, { 0, 1000 }, { -707, 707 },
    { -1000, 0 }, { -707, -707 }, { 0, -1000 }, { 707, -707 }
};

static const uint32_t SKY_COLORS[SKY_LEVELS] = {
    0xFF0B1026, 0xFF1B2A5E, 0xFF2F5BA8, 0xFF4A90D9, 0xFF7EC8F0, 0xFFA8E0FF
};

/* ******************************************************
 *      Helpers
 * ******************************************************/

/**
 * @brief  Sine of an angle, from a table with linear interpolation.
 * @param  deg: angle in degrees, 0..180
 * @retval sin(deg) x 1000
 */
static int32_t sinDeg(int32_t deg)
{
    int32_t i = deg / 10;
    int32_t f = deg % 10;

    if (f == 0)
        return SIN_TABLE[i];
    return SIN_TABLE[i] + (SIN_TABLE[i + 1] - SIN_TABLE[i]) * f / 10;
}

/**
 * @brief  Cosine of an angle.
 * @param  deg: angle in degrees, 0..180
 * @retval cos(deg) x 1000
 */
static int32_t cosDeg(int32_t deg)
{
    return deg <= 90 ? sinDeg(90 - deg) : -sinDeg(deg - 90);
}

/**
 * @brief  Draws a text with a given font, the current font is restored afterwards.
 * @param  x: left position
 * @param  y: top position
 * @param  font: font of the text
 * @param  color: text color
 * @param  background: background color
 * @param  text: string to draw
 * @retval None
 */
static void drawText(int32_t x, int32_t y, sFONT *font, uint32_t color, uint32_t background, const char *text)
{
    sFONT *previous = BSP_LCD_GetFont();

    BSP_LCD_SetFont(font);
    BSP_LCD_SetTextColor(color);
    BSP_LCD_SetBackColor(background);
    BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
    BSP_LCD_SetFont(previous);
}

/**
 * @brief  Same as drawText, with the text centered on x.
 * @retval None
 */
static void drawTextCentered(int32_t x, int32_t y, sFONT *font, uint32_t color, uint32_t background, const char *text)
{
    drawText(x - (int32_t)(strlen(text) * font->Width) / 2, y, font, color, background, text);
}

/**
 * @brief  Rounds a position to a multiple of FLOWER_STEP_PX, to redraw less often.
 * @param  value: position in pixels
 * @retval rounded position
 */
static int32_t snapToStep(int32_t value)
{
    int32_t half = value >= 0 ? FLOWER_STEP_PX / 2 : -FLOWER_STEP_PX / 2;

    return (value + half) / FLOWER_STEP_PX * FLOWER_STEP_PX;
}

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

/* ******************************************************
 *      STOP screen
 * ******************************************************/

/**
 * @brief  Draws the STOP screen with the instructions to start.
 * @retval None
 */
static void drawStop(void)
{
    clearScreen(DBG_BACKGROUND);

    BSP_LCD_SetTextColor(DBG_ACCENT);
    BSP_LCD_FillRect(0, 0, SCREEN_WIDTH, 30);
    drawTextCentered(SCREEN_CENTER_X, 5, &Font20, DBG_BACKGROUND, DBG_ACCENT, "LIGHT TRACKER");

    /* Stop symbol: red ring around a square */
    BSP_LCD_SetTextColor(0xFFE53935);
    BSP_LCD_FillCircle(SCREEN_CENTER_X, 110, 52);
    BSP_LCD_SetTextColor(DBG_PANEL);
    BSP_LCD_FillCircle(SCREEN_CENTER_X, 110, 45);
    BSP_LCD_SetTextColor(0xFFE53935);
    BSP_LCD_FillRect(SCREEN_CENTER_X - 22, 88, 44, 44);

    drawTextCentered(SCREEN_CENTER_X, 178, &Font24, 0xFFE53935, DBG_BACKGROUND, "STOP");
    drawTextCentered(SCREEN_CENTER_X, 212, &Font16, DBG_DIM, DBG_BACKGROUND, "Tracker en veille");

    /* How to start: user button or a light */
    BSP_LCD_SetTextColor(DBG_PANEL);
    BSP_LCD_FillRect(14, 244, 212, 30);
    BSP_LCD_FillRect(14, 282, 212, 30);

    BSP_LCD_SetTextColor(DBG_ACCENT);
    BSP_LCD_FillCircle(34, 259, 8);
    BSP_LCD_SetTextColor(DBG_PANEL);
    BSP_LCD_FillCircle(34, 259, 4);
    drawText(54, 251, &Font16, DBG_TEXT, DBG_PANEL, "Bouton USER");

    BSP_LCD_SetTextColor(0xFFFFD60A);
    BSP_LCD_FillCircle(34, 297, 6);
    for (int i = 0; i < 8; i++) {
        BSP_LCD_DrawLine(34 + 9 * RAY_DIR[i][0] / 1000, 297 - 9 * RAY_DIR[i][1] / 1000,
                         34 + 13 * RAY_DIR[i][0] / 1000, 297 - 13 * RAY_DIR[i][1] / 1000);
    }
    drawText(54, 289, &Font16, DBG_TEXT, DBG_PANEL, "ou une lumiere");
}

/* ******************************************************
 *      Animation screen: sky, sun and sunflower
 * ******************************************************/

/**
 * @brief  Draws the sun (bigger with more light) or the moon when it is dark.
 * @param  x: horizontal position of the center
 * @param  level: sky level, 0 (night) to SKY_LEVELS - 1
 * @param  sky: sky color, used to carve the moon crescent
 * @retval None
 */
static void drawSun(int32_t x, int32_t level, uint32_t sky)
{
    if (level == 0) {
        BSP_LCD_SetTextColor(0xFFE8E8D0);
        BSP_LCD_FillCircle(x, SUN_Y, 16);
        BSP_LCD_SetTextColor(sky);
        BSP_LCD_FillCircle(x + 8, SUN_Y - 5, 14);
        return;
    }

    int32_t radius = 8 + level * 3;

    BSP_LCD_SetTextColor(level >= 4 ? 0xFFFFB300 : 0xFFFFD60A);
    BSP_LCD_FillCircle(x, SUN_Y, radius);
    for (int i = 0; i < 8; i++) {
        BSP_LCD_DrawLine(x + (radius + SUN_RAY_MIN) * RAY_DIR[i][0] / 1000,
                         SUN_Y - (radius + SUN_RAY_MIN) * RAY_DIR[i][1] / 1000,
                         x + (radius + SUN_RAY_MAX) * RAY_DIR[i][0] / 1000,
                         SUN_Y - (radius + SUN_RAY_MAX) * RAY_DIR[i][1] / 1000);
    }
}

/**
 * @brief  Draws the sunflower: bent stem, two leaves, petals and a smiling face.
 * @param  head_x: horizontal position of the flower head
 * @retval None
 */
static void drawFlower(int32_t head_x)
{
    int32_t lean = head_x - FLOWER_X;
    int32_t stem_height = SKY_HEIGHT - FLOWER_Y;

    BSP_LCD_SetTextColor(STEM_COLOR);
    for (int o = -1; o <= 1; o++)
        BSP_LCD_DrawLine(FLOWER_X + o, SKY_HEIGHT, head_x + o, FLOWER_Y);

    BSP_LCD_SetTextColor(LEAF_COLOR);
    BSP_LCD_FillEllipse(FLOWER_X + lean * (SKY_HEIGHT - 215) / stem_height - 16, 215, 14, 6);
    BSP_LCD_FillEllipse(FLOWER_X + lean * (SKY_HEIGHT - 205) / stem_height + 16, 205, 14, 6);

    BSP_LCD_SetTextColor(PETAL_COLOR);
    for (int i = 0; i < 8; i++) {
        BSP_LCD_FillCircle(head_x + PETAL_DISTANCE * RAY_DIR[i][0] / 1000,
                           FLOWER_Y - PETAL_DISTANCE * RAY_DIR[i][1] / 1000,
                           PETAL_RADIUS);
    }

    BSP_LCD_SetTextColor(FLOWER_CORE_COLOR);
    BSP_LCD_FillCircle(head_x, FLOWER_Y, FLOWER_CORE_RADIUS);

    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_FillCircle(head_x - 8, FLOWER_Y - 5, 3);
    BSP_LCD_FillCircle(head_x + 8, FLOWER_Y - 5, 3);
    BSP_LCD_SetTextColor(PETAL_COLOR);
    BSP_LCD_DrawLine(head_x - 8, FLOWER_Y + 6, head_x - 4, FLOWER_Y + 10);
    BSP_LCD_DrawLine(head_x - 4, FLOWER_Y + 10, head_x + 4, FLOWER_Y + 10);
    BSP_LCD_DrawLine(head_x + 4, FLOWER_Y + 10, head_x + 8, FLOWER_Y + 6);
}

/**
 * @brief  Draws the animation mode. The sky gets brighter with the light, the sun shows where the
 *         light comes from and the sunflower turns its head with the servo.
 *         Only what changed is redrawn, except when entering the mode.
 * @param  msg: state, measures and angle to display
 * @param  clear: true on entering the mode, to redraw the whole screen
 * @retval None
 */
void drawAnimation(DisplayMsg *msg, bool clear)
{
    static int32_t prev_level = 0;
    static int32_t prev_sun_x = 0;
    static int32_t prev_head_x = 0;
    static int32_t prev_percent = 0;

    int32_t angle = clamp(msg->angle, ANGLE_MIN_DEG, ANGLE_MAX_DEG);
    int32_t total = clamp((msg->left + msg->right) / 2, 0, ADC_MAX);
    int32_t level = clamp(total * SKY_LEVELS / (ADC_MAX + 1), 0, SKY_LEVELS - 1);
    int32_t percent = total * 100 / ADC_MAX;
    int32_t sun_x = snapToStep(SCREEN_CENTER_X + angle * SUN_TRAVEL_PX / ANGLE_MAX_DEG);
    int32_t head_x = snapToStep(FLOWER_X + angle * FLOWER_TRAVEL_PX / ANGLE_MAX_DEG);
    uint32_t sky = SKY_COLORS[level];
    bool redraw_all = clear || level != prev_level;
    char line[16];

    if (redraw_all) {
        BSP_LCD_SetTextColor(sky);
        BSP_LCD_FillRect(0, 0, SCREEN_WIDTH, SKY_HEIGHT);
        BSP_LCD_SetTextColor(GROUND_COLOR);
        BSP_LCD_FillRect(0, SKY_HEIGHT, SCREEN_WIDTH, BSP_LCD_GetYSize() - SKY_HEIGHT);
        BSP_LCD_SetTextColor(GROUND_EDGE_COLOR);
        BSP_LCD_FillRect(0, SKY_HEIGHT, SCREEN_WIDTH, 6);
    }

    if (redraw_all || sun_x != prev_sun_x) {
        if (!redraw_all) {
            BSP_LCD_SetTextColor(sky);
            BSP_LCD_FillRect(prev_sun_x - SUN_ERASE_HALF, SUN_Y - SUN_ERASE_HALF, SUN_ERASE_HALF * 2, SUN_ERASE_HALF * 2);
        }
        drawSun(sun_x, level, sky);
    }

    if (redraw_all || head_x != prev_head_x) {
        if (!redraw_all) {
            BSP_LCD_SetTextColor(sky);
            BSP_LCD_FillRect(0, 100, SCREEN_WIDTH, SKY_HEIGHT - 100);
        }
        drawFlower(head_x);
    }

    if (redraw_all || percent != prev_percent) {
        snprintf(line, sizeof line, "LIGHT %3ld%%", (long)percent);
        drawTextCentered(SCREEN_CENTER_X, 285, &Font16, LCD_COLOR_WHITE, GROUND_COLOR, line);
    }

    prev_level = level;
    prev_sun_x = sun_x;
    prev_head_x = head_x;
    prev_percent = percent;
}

/* ******************************************************
 *      Debug screen: gauges, gap and servo
 * ******************************************************/

/**
 * @brief  Draws the needle of a gauge. Level 0 points to the left, ADC_MAX to the right.
 * @param  cx: horizontal position of the gauge center
 * @param  level: sensor value, from 0 to ADC_MAX
 * @param  color: needle color, the panel color erases it
 * @retval None
 */
static void drawNeedle(int32_t cx, int32_t level, uint32_t color)
{
    int32_t deg = 180 - clamp(level, 0, ADC_MAX) * 180 / ADC_MAX;
    int32_t x = cx + GAUGE_NEEDLE_LEN * cosDeg(deg) / 1000;
    int32_t y = GAUGE_CY - GAUGE_NEEDLE_LEN * sinDeg(deg) / 1000;

    BSP_LCD_SetTextColor(color);
    BSP_LCD_DrawLine(cx, GAUGE_CY, x, y);
    BSP_LCD_DrawLine(cx + 1, GAUGE_CY, x + 1, y);
    BSP_LCD_DrawLine(cx, GAUGE_CY + 1, x, y + 1);
}

/**
 * @brief  Draws the fixed part of a light gauge: panel, colored arc, ticks and label.
 * @param  cx: horizontal position of the gauge center
 * @param  label: name under the gauge
 * @retval None
 */
static void drawGaugeFace(int32_t cx, const char *label)
{
    BSP_LCD_SetTextColor(DBG_PANEL);
    BSP_LCD_FillRect(cx - 56, 58, 112, 112);

    for (int deg = 0; deg <= 180; deg += 2) {
        int32_t from_left = 180 - deg;
        BSP_LCD_SetTextColor(from_left < 60 ? DBG_LEFT : from_left < 120 ? 0xFFFFD60A : 0xFFFF5A36);
        BSP_LCD_FillRect(cx + GAUGE_RADIUS * cosDeg(deg) / 1000 - 1,
                         GAUGE_CY - GAUGE_RADIUS * sinDeg(deg) / 1000 - 1, 3, 3);
    }

    BSP_LCD_SetTextColor(DBG_DIM);
    for (int deg = 0; deg <= 180; deg += 30) {
        BSP_LCD_DrawLine(cx + (GAUGE_RADIUS - 9) * cosDeg(deg) / 1000, GAUGE_CY - (GAUGE_RADIUS - 9) * sinDeg(deg) / 1000,
                         cx + (GAUGE_RADIUS - 5) * cosDeg(deg) / 1000, GAUGE_CY - (GAUGE_RADIUS - 5) * sinDeg(deg) / 1000);
    }

    drawTextCentered(cx, 130, &Font12, DBG_DIM, DBG_PANEL, label);
}

/**
 * @brief  Moves the needle of a gauge and refreshes its value, nothing is drawn if the level did not change.
 * @param  cx: horizontal position of the gauge center
 * @param  level: sensor value, from 0 to ADC_MAX
 * @param  previous: last level drawn, INT32_MIN when none
 * @retval None
 */
static void updateGauge(int32_t cx, int32_t level, int32_t *previous)
{
    char line[8];

    level = clamp(level, 0, ADC_MAX);
    if (level == *previous)
        return;

    if (*previous != INT32_MIN)
        drawNeedle(cx, *previous, DBG_PANEL);
    drawNeedle(cx, level, DBG_TEXT);
    BSP_LCD_SetTextColor(DBG_ACCENT);
    BSP_LCD_FillCircle(cx, GAUGE_CY, 5);

    snprintf(line, sizeof line, "%4ld", (long)level);
    drawTextCentered(cx, 146, &Font16, DBG_TEXT, DBG_PANEL, line);
    *previous = level;
}

/**
 * @brief  Draws the debug mode: title, two light gauges, the gap between them as a centered bar,
 *         the servo angle on a slider and the pulse width sent to the servo.
 *         Only what changed is redrawn, except when entering the mode.
 * @param  msg: state, measures and angle to display
 * @param  clear: true on entering the mode, to redraw the whole screen
 * @retval None
 */
void drawDebug(DisplayMsg *msg, bool clear)
{
    static int32_t prev_left = INT32_MIN;
    static int32_t prev_right = INT32_MIN;
    static int32_t prev_diff = INT32_MIN;
    static int32_t prev_gap_bar = INT32_MIN;
    static int32_t prev_angle = INT32_MIN;
    char line[24];

    if (clear) {
        clearScreen(DBG_BACKGROUND);
        BSP_LCD_SetTextColor(DBG_ACCENT);
        BSP_LCD_FillRect(0, 0, SCREEN_WIDTH, 30);
        drawText(10, 5, &Font20, DBG_BACKGROUND, DBG_ACCENT, "DEBUG MODE");
        drawText(10, 38, &Font12, DBG_DIM, DBG_BACKGROUND, "LIGHT SENSORS");

        drawGaugeFace(62, "LEFT");
        drawGaugeFace(178, "RIGHT");

        BSP_LCD_SetTextColor(DBG_PANEL);
        BSP_LCD_FillRect(6, 178, 228, 58);
        drawText(14, 184, &Font12, DBG_DIM, DBG_PANEL, "GAP  LEFT - RIGHT");
        BSP_LCD_SetTextColor(DBG_DIM);
        BSP_LCD_DrawRect(14, 206, 212, 20);

        BSP_LCD_SetTextColor(DBG_PANEL);
        BSP_LCD_FillRect(6, 244, 228, 60);
        drawText(14, 250, &Font12, DBG_DIM, DBG_PANEL, "SERVO ANGLE");
        BSP_LCD_SetTextColor(DBG_DIM);
        BSP_LCD_DrawRect(14, 272, 212, 14);

        drawTextCentered(SCREEN_CENTER_X, 308, &Font12, DBG_DIM, DBG_BACKGROUND, "USER BUTTON: SWITCH MODE");

        prev_left = prev_right = prev_diff = prev_gap_bar = prev_angle = INT32_MIN;
    }

    updateGauge(62, msg->left, &prev_left);
    updateGauge(178, msg->right, &prev_right);

    if (msg->diff != prev_diff) {
        int32_t diff = clamp(msg->diff, -ADC_MAX, ADC_MAX);
        int32_t bar = diff * BAR_HALF_WIDTH / ADC_MAX;

        snprintf(line, sizeof line, "%+6ld", (long)diff);
        drawText(150, 182, &Font16, diff >= 0 ? DBG_LEFT : DBG_RIGHT, DBG_PANEL, line);

        if (bar != prev_gap_bar) {
            BSP_LCD_SetTextColor(DBG_PANEL);
            BSP_LCD_FillRect(15, 207, 210, 18);
            if (bar > 0) {
                BSP_LCD_SetTextColor(DBG_LEFT);
                BSP_LCD_FillRect(SCREEN_CENTER_X - bar, 207, bar, 18);
            } else if (bar < 0) {
                BSP_LCD_SetTextColor(DBG_RIGHT);
                BSP_LCD_FillRect(SCREEN_CENTER_X, 207, -bar, 18);
            }
            BSP_LCD_SetTextColor(DBG_TEXT);
            BSP_LCD_DrawVLine(SCREEN_CENTER_X, 206, 20);
            prev_gap_bar = bar;
        }
        prev_diff = msg->diff;
    }

    if (msg->angle != prev_angle) {
        int32_t angle = clamp(msg->angle, ANGLE_MIN_DEG, ANGLE_MAX_DEG);
        int32_t marker = SCREEN_CENTER_X + angle * BAR_HALF_WIDTH / ANGLE_MAX_DEG;
        int32_t pulse_us = PULSE_CENTER_US + angle * PULSE_PER_90DEG_US / 90;

        snprintf(line, sizeof line, "%+4ld deg", (long)angle);
        drawText(120, 248, &Font16, DBG_TEXT, DBG_PANEL, line);

        BSP_LCD_SetTextColor(DBG_PANEL);
        BSP_LCD_FillRect(15, 273, 210, 12);
        BSP_LCD_SetTextColor(DBG_DIM);
        BSP_LCD_DrawVLine(SCREEN_CENTER_X, 273, 12);
        BSP_LCD_SetTextColor(DBG_ACCENT);
        BSP_LCD_FillRect(marker - 3, 273, 6, 12);

        snprintf(line, sizeof line, "PULSE %4ld us", (long)pulse_us);
        drawText(14, 290, &Font12, DBG_TEXT, DBG_PANEL, line);
        prev_angle = msg->angle;
    }
}

/* ******************************************************
 *      Task
 * ******************************************************/

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
