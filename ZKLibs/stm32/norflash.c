#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_fsmc.h"
#include "semphr.h"
#include "norflash.h"

static xSemaphoreHandle __semaphore;

void NorFlashInit(void) {
	if (__semaphore == NULL) {
		FSMC_NOR_Init();
		vSemaphoreCreateBinary(__semaphore);
	}
}

void NorFlashWrite(uint32_t flash, const short *ram, int len) {

	if (xSemaphoreTake(__semaphore, configTICK_RATE_HZ * 5) == pdTRUE) {
		while(FSMC_NOR_EraseSector(flash) != NOR_SUCCESS);
//		FSMC_NOR_EraseSector(flash);
//		FSMC_NOR_WriteBuffer(ram, flash, len);
		while(FSMC_NOR_WriteBuffer(ram, flash, len) != NOR_SUCCESS);
		xSemaphoreGive(__semaphore);
	}
}

void NorFlashEraseParam(uint32_t flash) {
	if (xSemaphoreTake(__semaphore, configTICK_RATE_HZ * 5) == pdTRUE) {
		while(FSMC_NOR_EraseSector(flash) != NOR_SUCCESS);
		//FSMC_NOR_EraseSector(flash);
		xSemaphoreGive(__semaphore);
	}
}

void NorFlashEraseChip(void) {

	if (xSemaphoreTake(__semaphore, configTICK_RATE_HZ * 5) == pdTRUE) {
		while(FSMC_NOR_EraseChip() != NOR_SUCCESS);
		xSemaphoreGive(__semaphore);
	}
}

void NorFlashRead(uint32_t flash, short *ram, int len) {
	if (xSemaphoreTake(__semaphore, configTICK_RATE_HZ * 5) == pdTRUE) {
		FSMC_NOR_ReadBuffer(ram, flash, len);
		xSemaphoreGive(__semaphore);
	}
}


bool NorFlashMutexLock(uint32_t time) {
	return (xSemaphoreTake(__semaphore, time) == pdTRUE);

}
void NorFlashMutexUnlock(void) {
	xSemaphoreGive(__semaphore);
}




