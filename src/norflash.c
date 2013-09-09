#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_fsmc.h"
#include  "semphr.h"

static xSemaphoreHandle __flashSemaphore;

void FlashRuntimeInit() {
	vSemaphoreCreateBinary(__flashSemaphore);
}

void FlashWrite(int flash, const char *ram, int len) {

	if( xSemaphoreTake(__flashSemaphore, configTICK_RATE_HZ*5) == pdTRUE) {
		FSMC_NOR_EraseSector(flash);	
	    FSMC_NOR_WriteBuffer(ram, flash, (len + 1)/2);
		xSemaphoreGive(__flashSemaphore);
	}
}

void FlashRead(int flash, char *ram, int len) {
	if( xSemaphoreTake(__flashSemaphore, configTICK_RATE_HZ*5) == pdTRUE) {
	FSMC_NOR_ReadBuffer(ram, flash, len/2);
		xSemaphoreGive(__flashSemaphore);
	}
}