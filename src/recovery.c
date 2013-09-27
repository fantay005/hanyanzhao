#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x_gpio.h"
#include "fsmc_nor.h"
#include "norflash.h"
#include "watchdog.h"


#define RECOVERY_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE )

void RecoveryInit(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
}

void __taskRecovery(void *nouse) {
	NorFlashMutexLock(configTICK_RATE_HZ * 10);
	FSMC_NOR_EraseSector(XFS_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(GSM_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(USER_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(SMS1_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(SMS2_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	NorFlashMutexUnlock();

	printf("Reboot From Default Configuration\n");

//	WatchdogFeed();
	WatchdogStopFeed();
	while(1);
//	while (1) {
//		NVIC_SystemReset();
//		vTaskDelay(configTICK_RATE_HZ/5);
//	}
}


void RecoveryToFactory(void) {
	char currentState;
	static unsigned long lastTick = 0;
	if (lastTick == 0xFFFFFFFF) {
		return;
	}
	currentState = GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_13);
	if (currentState == Bit_SET) {
		lastTick = xTaskGetTickCount();
	} else {
		if (xTaskGetTickCount() - lastTick > configTICK_RATE_HZ * 5) {
			xTaskCreate(__taskRecovery, (signed portCHAR *) "REC", RECOVERY_TASK_STACK_SIZE, (void *)'2', tskIDLE_PRIORITY + 20, NULL);
			lastTick = 0xFFFFFFFF;
		}
	}
}

