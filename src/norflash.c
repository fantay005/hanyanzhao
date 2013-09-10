#include "FreeRTOS.h"
#include "task.h"
#include "fsmc_nor.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_fsmc.h"
#include  "semphr.h"

static xSemaphoreHandle __flashSemaphore;

void FlashRuntimeInit() {
	vSemaphoreCreateBinary(__flashSemaphore);
}

void FlashWrite(long flash,  short *ram, int len) {

	if (xSemaphoreTake(__flashSemaphore, configTICK_RATE_HZ * 5) == pdTRUE) {
		FSMC_NOR_EraseSector(flash);
		FSMC_NOR_WriteBuffer(ram, flash, (len + 1) / 2);
		xSemaphoreGive(__flashSemaphore);
	}
}

void FlashRead(int flash, short *ram, int len) {
	if (xSemaphoreTake(__flashSemaphore, configTICK_RATE_HZ * 5) == pdTRUE) {
		FSMC_NOR_ReadBuffer(ram, flash, len / 2);
		xSemaphoreGive(__flashSemaphore);
	}
}

typedef struct {
	unsigned char xfsVolume;
	unsigned char xfsPause;
	unsigned char xfsType;
	unsigned char xfsTimes;
} broadcastPara;

typedef struct {
	unsigned char IMEI[15];
	unsigned char phoneNum[6][11];
	unsigned char *IP;
} systemPara;
