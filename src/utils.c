/*
 * utils.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Jon Gouspy
 */

/* Project */
#include "utils.h"

/**
 * @brief  Absolute value of a signed 32-bit integer.
 * @param  value: value to convert
 * @retval value without its sign
 */
int32_t abs(int32_t value)
{
	return value < 0 ? - value : value;
}

/**
 * @brief  Limits a value to a range.
 * @param  value: value to limit
 * @param  min: lower bound
 * @param  max: upper bound
 * @retval value, or the nearest bound when it is out of range
 */
int32_t clamp(int32_t value, int32_t min, int32_t max)
{
    return value < min ? min : value > max ? max : value;
}
