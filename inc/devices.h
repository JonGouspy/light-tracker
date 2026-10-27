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

/* Signal: 50 Hz frame, pulse width gives the position */
#define SERVO_PERIOD_US             20000
#define SERVO_PULSE_MIN_US          1000
#define SERVO_PULSE_MAX_US          2000

/* ******************************************************
 *      Globals
 * ******************************************************/
#ifdef DEFINE_GLOBALS

TIM_HandleTypeDef gTimHandle;
TIM_HandleTypeDef gServoTimHandle;
ADC_HandleTypeDef gAdcHandle;

#else

extern TIM_HandleTypeDef gTimHandle;
extern TIM_HandleTypeDef gServoTimHandle;
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
void initServoTimer(void);
void initLCD(void);

void Servo_Init(void);
void Servo_On(void);
void Servo_Off(void);
void Servo_Toggle(void);
void Servo_SetPulse(uint32_t pulse_us);
void Servo_TimerUpdate(void);

void SystemClock_Config(void);

#endif /* __DEVICES_H */


