#ifdef __LED__
#include <string.h>
#include "led_lowlevel.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_fsmc.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "font_dot_array.h"
#include "zklib.h"
#include "led_lowlevel.h"


static unsigned char __scanLine = 0;

void LedScanClearAll() {
#if LED_DRIVER_LEVEL==1
	memset(__scanBuffer, 0x00, sizeof(__scanBuffer));
#else
	memset(__scanBuffer, 0xFF, sizeof(__scanBuffer));
#endif
}


static inline void __ledScanHardwareInit() {
	GPIO_InitTypeDef GPIO_InitStructure;
	FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef FSMC_NORSRAMTimingInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	DMA_InitTypeDef DMA_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOG | RCC_APB2Periph_GPIOE |
						   RCC_APB2Periph_GPIOF, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 |
								  GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
								  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
								  GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* SRAM Address lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_12 | GPIO_Pin_13 |
								  GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								  GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NOE and NWE configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NE3 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	/* NE4 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	/* NBL0, NBL1 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	FSMC_NORSRAMTimingInitStructure.FSMC_AddressSetupTime = 0;  //FSMC_SetupTime
	FSMC_NORSRAMTimingInitStructure.FSMC_AddressHoldTime = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_DataSetupTime = 2;
	FSMC_NORSRAMTimingInitStructure.FSMC_BusTurnAroundDuration = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_CLKDivision = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_DataLatency = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_AccessMode = FSMC_AccessMode_A;


	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM4;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;
	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);

	GPIO_ResetBits(GPIOC, GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_7);
	GPIO_SetBits(GPIOC, GPIO_Pin_5);

	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel6_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM_TimeBaseStructure.TIM_Period = 92160 / LED_SCAN_MUX / 8;
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_PrescalerConfig(TIM2, 8, TIM_PSCReloadMode_Immediate);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 8192;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
	TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);

	DMA_DeInit(DMA1_Channel6);
	DMA_InitStructure.DMA_PeripheralBaseAddr = 0;
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)0X6C001000;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = LED_SCAN_LENGTH;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
	DMA_InitStructure.DMA_MemoryInc     = DMA_MemoryInc_Disable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Low;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Enable;
	DMA_Init(DMA1_Channel6, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);
}


void LedScanOnOff(bool isOn) {
	if (isOn) {
		TIM_Cmd(TIM2, ENABLE);
	} else {
		TIM_Cmd(TIM2, DISABLE);
		GPIO_SetBits(GPIOC, GPIO_Pin_5);
	}
}

bool LedScanSetScanBuffer(int mux, int x, unsigned char dat) {
	if (x >= LED_SCAN_LENGTH) {
		return false;
	}
	if (mux >= LED_SCAN_MUX) {
		return false;
	}
	__scanBuffer[mux][x] = dat;
	return true;
}

void LedScanInit() {
	LedScanClearAll();
	__scanBufferBit = BIT_BAND_ADDR_SRAM(__scanBuffer);
	__ledScanHardwareInit();
}

void TIM2_IRQHandler() {
	portBASE_TYPE tmp;
#define DMA1_Channel6_IT_Mask    ((uint32_t)(DMA_ISR_GIF6 | DMA_ISR_TCIF6 | DMA_ISR_HTIF6 | DMA_ISR_TEIF6))
	TIM2->SR = ~(0x00FF);
	DMA1_Channel6->CCR = 0x6042;
	DMA1->IFCR |= DMA1_Channel6_IT_Mask;
	DMA1_Channel6->CNDTR = LED_SCAN_LENGTH;
	DMA1_Channel6->CPAR = (uint32_t)(__scanBuffer[__scanLine]);
	tmp = GPIOC->ODR;
#if LED_OE_LEVEL==1
	tmp &= ~(1 << 5);
#elif LED_OE_LEVEL==0
	tmp |= (1 << 5);
#endif
	GPIOC->ODR = tmp;
	DMA1_Channel6->CCR = 0x6043;
}

void DMA1_Channel6_IRQHandler(void) {
	if (DMA_GetITStatus(DMA1_IT_TC6)) {
		portBASE_TYPE tmp;
		DMA_ClearITPendingBit(DMA1_IT_GL6);
		tmp = GPIOC->ODR;
#if LED_STROBE_PAUSE==1
		tmp &= ~(0x0F << 1);
		tmp |= __scanLine << 1 | (1 << 7);
#elif LED_STROBE_PAUSE==0
		tmp &= ~((0x0F << 1) | (1 << 7));
		tmp |= __scanLine << 1;
#endif
		GPIOC->ODR = tmp;
		if (++__scanLine >= LED_SCAN_MUX) {
			__scanLine = 0;
		}
#if LED_STROBE_PAUSE==1
		tmp &= ~(1 << 7);
#elif LED_STROBE_PAUSE==0
		tmp |= 1 << 7;
#endif
		GPIOC->ODR = tmp;
#if LED_OE_LEVEL==1
		tmp |= (1 << 5);
#elif LED_OE_LEVEL==0
		tmp &= ~(1 << 5);
#endif
		GPIOC->ODR = tmp;
	}
}

#endif
