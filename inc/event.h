/*
 * event.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

#ifndef EVENT_H_
#define EVENT_H_

/* Standard */
#include <stdint.h>

typedef enum {
	EVT_BUTTON,
	EVT_LIGHT
} EventType;

typedef struct {
	EventType type;
	int32_t left;
	int32_t right;
	int32_t diff;
} Event;

#endif /* EVENT_H_ */
