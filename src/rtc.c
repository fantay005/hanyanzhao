#include  "stm32f10x_exti.h"
#include  "stm32f10x_bkp.h"
#include  "stm32f10x_rcc.h"
#include  "stm32f10x_pwr.h"
#include  "stm32f10x_rtc.h"
#include  "stm32f10x_gpio.h"
#include  "misc.h"
#include "FreeRTOS.h"
#include  "semphr.h"
#include "task.h"

#define  RUN_OPEN 	GPIO_ResetBits(GPIOC, GPIO_Pin_0)
#define	 RUN_CLOSE	GPIO_SetBits(GPIOC, GPIO_Pin_0)

static xSemaphoreHandle __rtcSystemRunningSemaphore;

void RtcSystemRunningIndictor() {
	if( xSemaphoreTake(__rtcSystemRunningSemaphore, 0) == pdTRUE) {
		GPIO_WriteBit(GPIOC, GPIO_Pin_0, GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_0) == Bit_RESET ? Bit_SET : Bit_RESET);
	}
}


void RTC_IRQHandler(void)
{
	if (RTC_GetITStatus(RTC_IT_SEC) != RESET) {	
		portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
		RTC_ClearITPendingBit(RTC_IT_SEC);
		xSemaphoreGiveFromISR(__rtcSystemRunningSemaphore, &xHigherPriorityTaskWoken);
		if (xHigherPriorityTaskWoken) {		
			taskYIELD();
		}
  	}
}


void RTC_Configuration(void)
{

    PWR_BackupAccessCmd(ENABLE);        //开启RTC和后备寄存器的访问

    BKP_DeInit();
    RCC_LSEConfig(RCC_LSE_ON);         //设置外部低速晶振
    while(RESET == RCC_GetFlagStatus(RCC_FLAG_LSERDY));    //等待时钟稳定
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);      //设置LSE为RTC时钟
    RCC_RTCCLKCmd(ENABLE);          //使能RTC时钟
 
    RTC_WaitForSynchro();          //等待时钟与APB1时钟同步
    RTC_WaitForLastTask();          //等待最近一次对RTC寄存器的操作完成
    RTC_SetPrescaler(32767);         //设置RTC的预分频值
    RTC_WaitForLastTask();          //等待最近一次对RTC寄存器的操作完成
    RTC_SetAlarm(RTC_GetCounter() + 2);           //设置闹铃的值
    RTC_WaitForLastTask();

    RTC_ITConfig(RTC_IT_SEC,ENABLE);
    RTC_WaitForLastTask();
    RTC_ITConfig(RTC_IT_SEC,ENABLE);        //设置秒中断
    RTC_WaitForLastTask();
}


void RTC_Config(void)
{
	if(0xA5A5 != BKP_ReadBackupRegister(BKP_DR1)) {
		RTC_Configuration();
		BKP_WriteBackupRegister(BKP_DR1,0xA5A5);
	} else {
		if(RESET != RCC_GetFlagStatus(RCC_FLAG_LPWRRST)) { //如果低功耗复位
			;
		}
		if(RESET != RCC_GetFlagStatus(RCC_FLAG_PINRST)) {  //如果wakeup复位
			;
		}
		RCC_ClearFlag();
		RTC_ITConfig(RTC_IT_SEC,ENABLE);
		RTC_WaitForLastTask();
		RTC_ITConfig(RTC_IT_SEC,ENABLE);       //设置闹铃中断
		RTC_WaitForLastTask();
	}
}

void InitRTC()
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;
    EXTI_InitTypeDef  EXTI_InitStructure;

	vSemaphoreCreateBinary(__rtcSystemRunningSemaphore);

	GPIO_ResetBits(GPIOC, GPIO_Pin_0);
 	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitStructure.NVIC_IRQChannel = RTCAlarm_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Line = EXTI_Line17;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority =0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

	RTC_Config();
}


