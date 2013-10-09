#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

extern void RecoveryToFactory(void);
extern void WatchdogFeed(void);

/// Malloc failed hook for FreeRTOS.
void vApplicationMallocFailedHook(void) {
	volatile int exit = 0;
	while (! exit) {
		printf("vApplicationMallocFailedHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 2);
	}
}

/// Application idle hook for FreeRTOS.
void vApplicationIdleHook(void) {
	RecoveryToFactory();
	WatchdogFeed();
}
