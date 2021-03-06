#include <stdio.h>
#include "rtc.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_gpio.h"
#include "misc.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"



#define INDICTOR_LED_GPIO_PORT GPIOE
#define INDICTOR_LED_GPIO_PIN  GPIO_Pin_2

//#define GPRS_ENABLE_GPIO_PORT  GPIOB
//#define GPRS_ENABLE_GPIO_PIN   GPIO_Pin_6

//#define GPRS_ENABLE_GPIO_PORT  GPIOB
//#define GPRS_ENABLE_GPIO_PIN   GPIO_Pin_9

static xSemaphoreHandle __rtcSystemRunningSemaphore;

extern char TCPStatus(char type, char value);

bool RtcWaitForSecondInterruptOccured(uint32_t time) {
//	static int count = 0;
	if (xSemaphoreTake(__rtcSystemRunningSemaphore, time) == pdTRUE) {
		
		GPIO_WriteBit(INDICTOR_LED_GPIO_PORT, INDICTOR_LED_GPIO_PIN,
		GPIO_ReadOutputDataBit(INDICTOR_LED_GPIO_PORT, INDICTOR_LED_GPIO_PIN) == Bit_RESET ? Bit_SET : Bit_RESET);

//		GPIO_WriteBit(GPRS_ENABLE_GPIO_PORT, GPRS_ENABLE_GPIO_PIN,
//		GPIO_ReadOutputDataBit(GPRS_ENABLE_GPIO_PORT, GPRS_ENABLE_GPIO_PIN) == Bit_RESET ? Bit_SET : Bit_RESET);

//		printf("RtcSystemRunningIndictor: %d\n", ++count);
		return true;
	}
	return false;
}


void RTC_IRQHandler(void) {
	if (RTC_GetITStatus(RTC_IT_SEC) != RESET) {
		portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
		RTC_ClearITPendingBit(RTC_IT_SEC);
		xSemaphoreGiveFromISR(__rtcSystemRunningSemaphore, &xHigherPriorityTaskWoken);
		if (xHigherPriorityTaskWoken) {
			taskYIELD();
		}
	}
}


static void rtcConfiguration(void) {

	PWR_BackupAccessCmd(ENABLE);        //开启RTC和后备寄存器的访问

	BKP_DeInit();
	RCC_LSEConfig(RCC_LSE_ON);         //设置外部低速晶振
	while (RESET == RCC_GetFlagStatus(RCC_FLAG_LSERDY));   //等待时钟稳定
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);      //设置LSE为RTC时钟
	RCC_RTCCLKCmd(ENABLE);          //使能RTC时钟

	RTC_ITConfig(RTC_IT_OW, DISABLE);
	RTC_WaitForLastTask();
	RTC_ITConfig(RTC_IT_ALR, DISABLE);
	RTC_WaitForLastTask();

	RTC_WaitForSynchro();          //等待时钟与APB1时钟同步
	RTC_WaitForLastTask();          //等待最近一次对RTC寄存器的操作完成
		RTC_ITConfig(RTC_IT_SEC, ENABLE);      //设置闹铃中断
	RTC_WaitForLastTask();
	RTC_SetPrescaler(32767);         //设置RTC的预分频值
	RTC_WaitForLastTask();          //等待最近一次对RTC寄存器的操作完成
//	RTC_SetAlarm(RTC_GetCounter() + 2);           //设置闹铃的值
//	RTC_WaitForLastTask();
}


static void rtcConfig(void) {
	if (0xA5A5 != BKP_ReadBackupRegister(BKP_DR1)) {
		rtcConfiguration();
		BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
	} else {

		PWR_BackupAccessCmd(ENABLE);        //开启RTC和后备寄存器的访问
		if (RESET != RCC_GetFlagStatus(RCC_FLAG_LPWRRST)) { //如果低功耗复位
			;
		}
		if (RESET != RCC_GetFlagStatus(RCC_FLAG_PINRST)) { //如果wakeup复位
			;
		}
	}

	RCC_ClearFlag();
	RTC_WaitForLastTask();
	RTC_ITConfig(RTC_IT_OW | RTC_IT_ALR, DISABLE);
	RTC_WaitForLastTask();
	RTC_ITConfig(RTC_IT_SEC, ENABLE);      //设置闹铃中断
	RTC_WaitForLastTask();
}

void RtcInit(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	NVIC_InitTypeDef  NVIC_InitStructure;

	vSemaphoreCreateBinary(__rtcSystemRunningSemaphore);
	rtcConfig();

	GPIO_ResetBits(INDICTOR_LED_GPIO_PORT, INDICTOR_LED_GPIO_PIN);
	GPIO_InitStructure.GPIO_Pin =  INDICTOR_LED_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(INDICTOR_LED_GPIO_PORT, &GPIO_InitStructure);

//	GPIO_SetBits(GPRS_ENABLE_GPIO_PORT, GPRS_ENABLE_GPIO_PIN);
//	GPIO_InitStructure.GPIO_Pin =  GPRS_ENABLE_GPIO_PIN;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(GPRS_ENABLE_GPIO_PORT, &GPIO_InitStructure);
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void RtcSetTime(uint32_t seconds) {

	RTC_WaitForLastTask();
	RTC_SetCounter(seconds);
	RTC_WaitForLastTask();
}
