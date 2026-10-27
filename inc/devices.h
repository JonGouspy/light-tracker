/*
 * devices.h
 *
 */

#ifndef __DEVICES_H
#define __DEVICES_H

/* ******************************************************
 *      Includes
 * ******************************************************/

/* Standard */
#include <stdint.h>

/* Board */
#include "stm32f429i_discovery_lcd.h"

/* ******************************************************
 *      Constants
 * ******************************************************/

#define COLORS_NORMAL               0
#define COLORS_REVERSE              1

/* ******************************************************
 *      Servo (signal wire on PG13, shared with LED3)
 * ******************************************************/

/* Pin */
#define SERVO_PIN                   GPIO_PIN_13
#define SERVO_GPIO_PORT             GPIOG
#define SERVO_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOG_CLK_ENABLE()
#define SERVO_GPIO_CLK_DISABLE()    __HAL_RCC_GPIOG_CLK_DISABLE()

/* ******************************************************
 *      Globals
 * ******************************************************/
#ifdef DEFINE_GLOBALS

TIM_HandleTypeDef gTimHandle;
ADC_HandleTypeDef gAdcHandle;

#else

extern TIM_HandleTypeDef gTimHandle;
extern ADC_HandleTypeDef gAdcHandle;

#endif

/* ******************************************************
 *      Prototypes
 * ******************************************************/

void LCDdisplayText(int line, int color, char* text);
void HAL_LTDC_LineEvenCallback(LTDC_HandleTypeDef *hltdc);

void initGPIO(void);
void initADC(void);
void initTimer(void);
void initLCD(void);

void Servo_Init(void);

void SystemClock_Config(void);

#endif /* __DEVICES_H */


