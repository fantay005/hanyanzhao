#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "rtc.h"

extern void RecoveryToFactory(void);

void vApplicationMallocFailedHook(void) {
	volatile int exit = 0;
	while (! exit) {
		printf("vApplicationMallocFailedHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 2);
	}
}

void vApplicationIdleHook(void) {
//	RtcIsSecondInterruptOccured();
	RecoveryToFactory();
	WatchdogFeed();
}
