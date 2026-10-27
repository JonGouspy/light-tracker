/*
 * devices.h
 *
 */

#ifndef __DEVICES_H
#define __DEVICES_H

/* ******************************************************
 *      Includes
 * ******************************************************/

#include "stm32f429i_discovery_lcd.h"

/* ******************************************************
 *      Constants
 * ******************************************************/

#define COLORS_NORMAL   0
#define COLORS_REVERSE  1

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

void SystemClock_Config(void);

#endif /* __DEVICES_H */


