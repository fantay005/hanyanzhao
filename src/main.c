/* Standard includes. */
#include <stdio.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_flash.h"
#include "misc.h"

/* The check task uses the sprintf function so requires a little more stack. */
#define mainUART_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE + 50 )

/*
 * Configure the clocks, GPIO and other peripherals as required by the demo.
 */

static void prvSetupHardware( void );
extern void fputcSetup( void );
extern void vXfs( void *parameter );
extern void vGsm( void *parameter );
extern void SoundControl(void);

int main( void ) {
	prvSetupHardware();
	fputcSetup();

	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);

//	xTaskCreate( vUartPrint, ( signed portCHAR * ) "UART1", mainUART_TASK_STACK_SIZE, (void *)'1', tskIDLE_PRIORITY + 1, NULL );
	xTaskCreate( vXfs, ( signed portCHAR * ) "XFS", mainUART_TASK_STACK_SIZE, (void *)'2', tskIDLE_PRIORITY + 2, NULL );
	xTaskCreate( vGsm, ( signed portCHAR * ) "GSM", mainUART_TASK_STACK_SIZE, (void *)'3', tskIDLE_PRIORITY + 3, NULL );
	/* Start the scheduler. */
	vTaskStartScheduler();
	return 0;
}


static void prvSetupHardware( void ) {
	ErrorStatus HSEStartUpStatus;
	/* RCC system reset(for debug purpose) */
	RCC_DeInit();
	/* Enable HSE */
	RCC_HSEConfig(RCC_HSE_ON);
	/* Wait till HSE is ready */
	HSEStartUpStatus = RCC_WaitForHSEStartUp();
	if(HSEStartUpStatus == SUCCESS) {
		/* Enable Prefetch Buffer */
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
		/* Flash 2 wait state */
		FLASH_SetLatency(FLASH_Latency_2);
		/* HCLK = SYSCLK */
		RCC_HCLKConfig(RCC_SYSCLK_Div1);
		/* PCLK2 = HCLK */
		RCC_PCLK2Config(RCC_HCLK_Div1);
		/* PCLK1 = HCLK/2 */
		RCC_PCLK1Config(RCC_HCLK_Div2);
		/* PLLCLK = 8MHz * 9 = 72 MHz */
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
		/* Enable PLL */
		RCC_PLLCmd(ENABLE);
		/* Wait till PLL is ready */
		while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {}
		/* Select PLL as system clock source */
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		/* Wait till PLL is used as system clock source */
		while(RCC_GetSYSCLKSource() != 0x08) {}
	}
	/* Enable FSMC, GPIOD, GPIOE, GPIOF, GPIOG and AFIO clocks */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
	                       RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
	                       RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOF |
	                       RCC_APB2Periph_GPIOG | RCC_APB2Periph_AFIO  |
	                       RCC_APB2Periph_USART1 | RCC_APB2Periph_SPI1
	                       , ENABLE);
	/* Enable peripheral clocks --------------------------------------------------*/

	/* Enable DMA1 clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	/* Enable USART2 clock */
	/* Enable UART4 clock */
	/* TIM2 clock enable */
	/* TIM3 clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 |
	                       RCC_APB1Periph_TIM2 |
	                       RCC_APB1Periph_USART2|
	                       RCC_APB1Periph_USART3|
	                       RCC_APB1Periph_UART4,ENABLE);

	SoundControl();
}
/*-----------------------------------------------------------*/


#ifdef  DEBUG
/* Keep the linker happy. */
void assert_failed( unsigned portCHAR* pcFile, unsigned portLONG ulLine ) {
	for( ;; ) {
	}
}
#endif

