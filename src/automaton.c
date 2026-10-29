/*
 * automaton.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

/* Standard */
#include <assert.h>
#include <stdbool.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "queue.h"

/* Project */
#include "automaton.h"
#include "display.h"
#include "event.h"
#include "servo.h"
#include "utils.h"

/* Links the sensors (mq_event), the display (mq_display) and the servo (target_angle) */
Automaton gAutomaton;

/**
 * @brief  Computes the next state from the received event.
 *         Button: STOP -> ANIM -> DEBUG -> ANIM.
 *         Light gap above START_THRESHOLD: STOP -> ANIM.
 * @param  state: current state
 * @param  evt: event just received
 * @retval new state, unchanged when the event triggers no transition
 */
static State nextState(State state, const Event *evt)
{
    bool button = evt->type == EVT_BUTTON;

    switch (state)
    {
    case STATE_STOP:
        if (button || abs(evt->diff) > START_THRESHOLD)
        	return STATE_TRACK_ANIM;
        break;
    case STATE_TRACK_ANIM:
        if (button)
        	return STATE_TRACK_DEBUG;
        break;
    case STATE_TRACK_DEBUG:
        if (button)
        	return STATE_TRACK_ANIM;
        break;
    default:
        break;
    }

    return state;
}

/**
 * @brief  Moves the target angle to cancel the gap between the sensors.
 *         Does nothing inside the dead zone, the angle stays within the servo limits.
 * @param  diff: gap between the left and right sensors
 * @retval None
 */
static void trackLight(int32_t diff)
{
	if (abs(diff) <= TRACKING_DEAD_ZONE)
		return;

	int32_t step = TRACKING_DIRECTION * diff / TRACKING_GAIN_DIVISOR;
	if (step == 0)
		step = diff > 0 ? TRACKING_DIRECTION : -TRACKING_DIRECTION;
	step = clamp(step, -TRACKING_MAX_STEP_DEG, TRACKING_MAX_STEP_DEG);
	int32_t angle = gAutomaton.target_angle + step;
	gAutomaton.target_angle = clamp(angle, ANGLE_MIN_DEG, ANGLE_MAX_DEG);
}

/**
 * @brief  Sends the state, the latest measures and the target angle to the display.
 *         The message is dropped when the display queue is full.
 * @param  state: current state
 * @param  measure: latest event received from the sensors
 * @retval None
 */
static void publishDisplay(State state, const Event *measure)
{
    DisplayMsg msg = {
    		state, measure->left,
			measure->right,
			measure->diff,
			gAutomaton.target_angle
    };

    xQueueSend(gAutomaton.mq_display, &msg, 0);
}

/**
 * @brief  Automaton task. Waits for a sensor event for at most one servo period,
 *         updates the state, tracks the light when not stopped, and
 *         refreshes the display when an event arrived or the angle moved.
 * @param  unused: not used, the queues and the angle are shared through gAutomaton
 * @retval None
 */
void vAutomatonTask(void *unused)
{
	assert(gAutomaton.mq_event != NULL);

	State state = STATE_STOP;
	Event latest = { 0 };

	publishDisplay(state, &latest);

	for (;;)
	{
		Event event;
		bool received = xQueueReceive(gAutomaton.mq_event, &event, pdMS_TO_TICKS(SERVO_PERIOD_MS)) == pdTRUE;
        int32_t previous_angle = gAutomaton.target_angle;

		if (received)
		{
			latest = event;
			state = nextState(state, &event);
		}

		if (received && state != STATE_STOP)
			trackLight(latest.diff);

		if (received || gAutomaton.target_angle != previous_angle)
			publishDisplay(state, &latest);
	}
}
