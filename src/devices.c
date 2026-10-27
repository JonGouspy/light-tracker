/*
 * devices.c
 *
 */

/* Standard */
#include <stdbool.h>

/* Board */
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "../Components/ili9341/ili9341.h"

/* Project */
#define DEFINE_GLOBALS
#include "devices.h"

/* ******************************************************
 *      Global Context
 * ******************************************************/

LTDC_HandleTypeDef LtdcHandle;

__IO uint32_t Pending = 0;

static volatile uint32_t servoPulse = (SERVO_PULSE_MIN_US + SERVO_PULSE_MAX_US) / 2;
static bool servoHigh = false;

/* ******************************************************
 *      Display
 * ******************************************************/

/**
 * @brief  Displays a centered text on a line of the LCD.
 * @param  line: line number
 * @param  color: text color
 * @param  text: string to display
 * @retval None
 */
void LCDdisplayText(int line, int color, char* text)
{
    BSP_LCD_SetTextColor(color);
    BSP_LCD_DisplayStringAt(0, LINE(line), (uint8_t*)text, CENTER_MODE);
}

/**
 * @brief  Draws the title and the frame of the LCD.
 * @param  version: COLORS_NORMAL (dark) or COLORS_REVERSE (light)
 * @retval None
 */
void LCDdisplayInit(int version)
{
    uint32_t back_color = version == COLORS_NORMAL ? LCD_COLOR_DARKGRAY : LCD_COLOR_LIGHTGRAY;
    uint32_t text_color = version == COLORS_NORMAL ? LCD_COLOR_WHITE : LCD_COLOR_BLACK;
    uint32_t rect_color = version == COLORS_NORMAL ? LCD_COLOR_LIGHTGRAY : LCD_COLOR_DARKGRAY;

    int xRectPos = 10;
    int yRectPos = BSP_LCD_GetFont()->Height * 2 + 10;
    BSP_LCD_Clear(back_color);
    BSP_LCD_SetBackColor(back_color);
    LCDdisplayText(1, text_color, "-GISTRE27-");
    BSP_LCD_SetTextColor(rect_color);
    BSP_LCD_FillRect(xRectPos, yRectPos, BSP_LCD_GetXSize() - (xRectPos * 2), BSP_LCD_GetYSize() - yRectPos - 10);
    HAL_LTDC_ProgramLineEvent(&LtdcHandle, 0);
}

/**
 * @brief  Line Event callback.
 *         Reconfigures the position of layer 1 when requested, then
 *         reconfigures the line event.
 * @param  hltdc: pointer to a LTDC_HandleTypeDef structure that contains
 *                the configuration information for the specified LTDC.
 * @retval None
 */
void HAL_LTDC_LineEvenCallback(LTDC_HandleTypeDef *hltdc)
{
    if(Pending == 1)
    {
        HAL_LTDC_SetWindowPosition(&LtdcHandle, 0, 0, 0);

        Pending = 0;
    }
    HAL_LTDC_ProgramLineEvent(hltdc, 0);
}

/* ******************************************************
 *      Servo
 * ******************************************************/

/**
 * @brief  Configures the servo signal pin as push-pull output, low (idle).
 * @retval None
 */
void Servo_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    SERVO_GPIO_CLK_ENABLE();

    GPIO_InitStruct.Pin = SERVO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(SERVO_GPIO_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(SERVO_GPIO_PORT, SERVO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  Sets the servo signal pin high.
 * @retval None
 */
void Servo_On(void)
{
    HAL_GPIO_WritePin(SERVO_GPIO_PORT, SERVO_PIN, GPIO_PIN_SET);
}

/**
 * @brief  Sets the servo signal pin low.
 * @retval None
 */
void Servo_Off(void)
{
    HAL_GPIO_WritePin(SERVO_GPIO_PORT, SERVO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  Inverts the servo signal pin.
 * @retval None
 */
void Servo_Toggle(void)
{
    HAL_GPIO_TogglePin(SERVO_GPIO_PORT, SERVO_PIN);
}

/**
 * @brief  Sets the servo position through the pulse width, clamped to the allowed range.
 * @param  pulse_us: pulse width in microseconds (SERVO_PULSE_MIN_US..SERVO_PULSE_MAX_US)
 * @retval None
 */
void Servo_SetPulse(uint32_t pulse_us)
{
    if (pulse_us < SERVO_PULSE_MIN_US) pulse_us = SERVO_PULSE_MIN_US;
    if (pulse_us > SERVO_PULSE_MAX_US) pulse_us = SERVO_PULSE_MAX_US;
    servoPulse = pulse_us;
}

/**
 * @brief  Called on each TIM7 update event: raises the pin for the pulse width,
 *         then lowers it for the rest of the 20 ms frame (timer ticks at 1 MHz).
 * @retval None
 */
void Servo_TimerUpdate(void)
{
    uint32_t pulse = servoPulse;

    if (servoHigh)
    {
        Servo_Off();
        servoHigh = false;
        __HAL_TIM_SET_AUTORELOAD(&gServoTimHandle, SERVO_PERIOD_US - pulse - 1);
    }
    else
    {
        Servo_On();
        servoHigh = true;
        __HAL_TIM_SET_AUTORELOAD(&gServoTimHandle, pulse - 1);
    }
}

/* ******************************************************
 *      Initialization
 * ******************************************************/

/**
 * @brief  Initializes the GPIO: user button and servo signal pin.
 * @retval None
 */
void initGPIO(void)
{
    BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);
    Servo_Init();
}

/**
 * @brief  Initializes and configures the servo timer.
 *              Instance    : TIM7 (timer clock 90 MHz)
 *              Tick        : 1 us (prescaler 90)
 *              Period      : one frame, the first event comes after SERVO_PERIOD_US
 *              IT          : Enabled (two events per 20 ms frame)
 * @retval None
 */
void initServoTimer(void)
{
    __HAL_RCC_TIM7_CLK_ENABLE();

    gServoTimHandle.Instance = TIM7;
    gServoTimHandle.Init.Prescaler = 90 - 1;
    gServoTimHandle.Init.Period = SERVO_PERIOD_US - 1;
    gServoTimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    gServoTimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&gServoTimHandle);

    HAL_NVIC_SetPriority(TIM7_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);

    HAL_TIM_Base_Start_IT(&gServoTimHandle);
}

/**
 * @brief  Initializes and configures the ADC.
 *              Instance    : ADC1
 *              GPIO        : C1 (channel 11, left), A5 (channel 5, right), analog inputs
 *              Mode        : scan of 2 channels, continuous, software start, no DMA
 *              Clock       : PCLK2 / 8
 *              IT          : Enabled (one interrupt per conversion)
 * @retval None
 */
void initADC(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    ADC_ChannelConfTypeDef ADC_ChannelStruct = { 0 };

    __GPIOA_CLK_ENABLE();
    __GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(ADC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    __ADC1_CLK_ENABLE();
    gAdcHandle.Instance = ADC1;
    gAdcHandle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV8;
    gAdcHandle.Init.Resolution = ADC_RESOLUTION_12B;
    gAdcHandle.Init.ScanConvMode = ENABLE;
    gAdcHandle.Init.ContinuousConvMode = ENABLE;
    gAdcHandle.Init.DiscontinuousConvMode = DISABLE;
    gAdcHandle.Init.NbrOfDiscConversion = 0;
    gAdcHandle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    gAdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    gAdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    gAdcHandle.Init.NbrOfConversion = 2;
    gAdcHandle.Init.DMAContinuousRequests = DISABLE;
    gAdcHandle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&gAdcHandle);

    ADC_ChannelStruct.Channel = ADC_CHANNEL_11;
    ADC_ChannelStruct.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    ADC_ChannelStruct.Rank = 1;
    ADC_ChannelStruct.Offset = 0;
    HAL_ADC_ConfigChannel(&gAdcHandle, &ADC_ChannelStruct);

    ADC_ChannelStruct.Channel = ADC_CHANNEL_5;
    ADC_ChannelStruct.Rank = 2;
    HAL_ADC_ConfigChannel(&gAdcHandle, &ADC_ChannelStruct);

    HAL_ADC_Start_IT(&gAdcHandle);
}

/**
 * @brief  Initializes and configures the timer.
 *              Instance    : TIM2 (timer clock 90 MHz)
 *              Tick        : 45 MHz (prescaler 2)
 *              Period      : 1000 ticks
 *              IT          : Enabled
 * @retval None
 */
void initTimer(void)
{
    __TIM2_CLK_ENABLE();

    gTimHandle.Instance = TIM2;
    gTimHandle.Init.Period = 1000 - 1;
    gTimHandle.Init.Prescaler = (90000000 / 40000000) - 1;
    gTimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    gTimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    gTimHandle.Init.RepetitionCounter = 0;
    HAL_TIM_Base_Init(&gTimHandle);

    __HAL_TIM_ENABLE_IT(&gTimHandle, TIM_IT_UPDATE);

    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    HAL_TIM_Base_Start_IT(&gTimHandle);
}

/**
 * @brief  Initializes the LCD with two layers (blending) and selects the foreground layer.
 * @retval None
 */
void initLCD(void)
{
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LCD_BACKGROUND_LAYER, LCD_FRAME_BUFFER);
    BSP_LCD_LayerDefaultInit(LCD_FOREGROUND_LAYER, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(LCD_FOREGROUND_LAYER);
    BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
    BSP_LCD_DisplayOn();
    HAL_LTDC_ProgramLineEvent(&LtdcHandle, 0);
}

/**
 * @brief  System Clock Configuration
 *         Enables the power control clock and the voltage scaling (optimizes the
 *         power consumption when the device is clocked below the maximum frequency),
 *         starts the HSE oscillator and the PLL with HSE as source, activates the
 *         Over-Drive mode, then selects the PLL as system clock source and
 *         configures the HCLK, PCLK1 and PCLK2 dividers.
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSE)
 *            SYSCLK(Hz)                     = 180000000
 *            HCLK(Hz)                       = 180000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 4
 *            APB2 Prescaler                 = 2
 *            HSE Frequency(Hz)              = 8000000
 *            PLL_M                          = 8
 *            PLL_N                          = 360
 *            PLL_P                          = 2
 *            PLL_Q                          = 7
 *            VDD(V)                         = 3.3
 *            Main regulator output voltage  = Scale1 mode
 *            Flash Latency(WS)              = 5
 * @param  None
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 360;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    HAL_PWREx_EnableOverDrive();

    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}
