#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x_gpio.h"
#include "fsmc_nor.h"
#include "norflash.h"
#include "watchdog.h"

#define RECOVERY_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE )

/// \brief  初始化恢复出厂设置按键需要的CPU片上资源.
/// 把GPIOG_13设置成输入状态.
void RecoveryInit(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
}


/// \brief  执行恢复出厂设置的任务函数.
/// 擦除保存在Nor Flash中的数据, 并重启系统.
void __taskRecovery(void *nouse) {
	NorFlashMutexLock(configTICK_RATE_HZ * 4);
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
	FSMC_NOR_EraseSector(FLAG_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(FIX_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	NorFlashMutexUnlock();
	printf("Reboot From Default Configuration\n");
	WatchdogResetSystem();
	while (1);
}

/// \brief  检测条件并恢复出厂设置.
/// 检测恢复出厂设置按键PIN, 如果按住超过5秒创建执行恢复出厂设置的任务.
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
		if (xTaskGetTickCount() - lastTick > configTICK_RATE_HZ * 3) {
			xTaskCreate(__taskRecovery, (signed portCHAR *) "REC", RECOVERY_TASK_STACK_SIZE, (void *)'2', tskIDLE_PRIORITY + 20, NULL);
			lastTick = 0xFFFFFFFF;
		}
	}
}

