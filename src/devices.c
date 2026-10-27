/*
 * devices.c
 *
 */

//#include "main.h"
#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "../Components/ili9341/ili9341.h"
#include "stm32f429i_discovery_lcd.h"
#define  DEFINE_GLOBALS
#include "devices.h"

/* ******************************************************
 *      Global Context
 * ******************************************************/

LTDC_HandleTypeDef LtdcHandle;

__IO uint32_t Pending = 0;


/* ******************************************************
 *      Functions
 * ******************************************************/

void LCDdisplayText(int line, int color, char* text)
{
    BSP_LCD_SetTextColor(color);
    BSP_LCD_DisplayStringAt(0, LINE(line), (uint8_t*)text, CENTER_MODE);
}

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
 * @param  hltdc: pointer to a LTDC_HandleTypeDef structure that contains
 *                the configuration information for the specified LTDC.
 * @retval None
 */
void HAL_LTDC_LineEvenCallback(LTDC_HandleTypeDef *hltdc)
{
    if(Pending == 1)
    {
        /* reconfigure the layer1 position */
        HAL_LTDC_SetWindowPosition(&LtdcHandle, 0, 0, 0);

        Pending = 0;
    }
    /* Reconfigure line event */
    HAL_LTDC_ProgramLineEvent(hltdc, 0);
}

/* ******************************************************
 *      Initialization
 * ******************************************************/

void initGPIO(void)
{
    BSP_LED_Init(LED3);
    BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);
}

/**
 *
 * Initialize and configure ADC.
 *              Instance    : ADC1
 *              GPIO        : C1
 *              Channel     : 11
 *              IT          : Enabled
 *
 */
void initADC(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_ChannelConfTypeDef ADC_ChannelStruct;

    /* Configure GPIO C1 */
    __GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Add ADC1 IRQ to NVIC */
    HAL_NVIC_SetPriority(ADC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    /* Configure ADC */
    __ADC1_CLK_ENABLE();
    gAdcHandle.Instance = ADC1;
    gAdcHandle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV8;  // prescaler on TIM2
    gAdcHandle.Init.Resolution = ADC_RESOLUTION_12B;
    gAdcHandle.Init.ScanConvMode = DISABLE;
    gAdcHandle.Init.ContinuousConvMode = ENABLE;
    gAdcHandle.Init.DiscontinuousConvMode = DISABLE;
    gAdcHandle.Init.NbrOfDiscConversion = 0;
    gAdcHandle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;  // TIM2 is trigger
    gAdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    gAdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    gAdcHandle.Init.NbrOfConversion = 1;
    gAdcHandle.Init.DMAContinuousRequests = ENABLE;
    gAdcHandle.Init.EOCSelection = DISABLE;
    HAL_ADC_Init(&gAdcHandle);

    /* Configure ADC Channel */
    ADC_ChannelStruct.Channel = ADC_CHANNEL_11;
    ADC_ChannelStruct.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    ADC_ChannelStruct.Rank = 1;
    ADC_ChannelStruct.Offset = 0;
    HAL_ADC_ConfigChannel(&gAdcHandle, &ADC_ChannelStruct);

    /* Start ADC Conversion */
    HAL_ADC_Start_IT(&gAdcHandle);
}

/**
 *
 * Initialize and configure Timer.
 *              Instance    : TIM2
 *              Period      : 500
 *              IT          : Enabled
 *
 */
void initTimer(void)
{
    /* Enable Timer 2 clock */
    __TIM2_CLK_ENABLE();

    /* Timer 2 initialization and configuration */
    gTimHandle.Instance = TIM2;                             // base 180 MHz
    gTimHandle.Init.Period = 1000 - 1;                      // 180 KHz
    gTimHandle.Init.Prescaler = (90000000 / 40000000) - 1;  // 90 KHz
    gTimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    gTimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    gTimHandle.Init.RepetitionCounter = 0;
    HAL_TIM_Base_Init(&gTimHandle);

    /* Enable Timer interrupt */
    __HAL_TIM_ENABLE_IT(&gTimHandle, TIM_IT_UPDATE);

    /* Add TIM2 IRQ to NVIC */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    /* Start Timer 2 in interrupt mode */
    HAL_TIM_Base_Start_IT(&gTimHandle);
}

void initLCD(void)
{
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LCD_BACKGROUND_LAYER, LCD_FRAME_BUFFER);  // Configure 2 layers w/ Blending
    BSP_LCD_LayerDefaultInit(LCD_FOREGROUND_LAYER, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(LCD_FOREGROUND_LAYER);
    BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
    BSP_LCD_DisplayOn();
    HAL_LTDC_ProgramLineEvent(&LtdcHandle, 0);
}

/**
 * @brief  System Clock Configuration
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

    /* Enable Power Control clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 360;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* Activate the Over-Drive mode */
    HAL_PWREx_EnableOverDrive();

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}
