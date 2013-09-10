#include "FreeRTOS.h"

extern void RtcSystemRunningIndictor();

void vApplicationMallocFailedHook(void) {
	volatile int exit = 0;
	while (! exit) {
		printf("vApplicationMallocFailedHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 2);
	}
}

void vApplicationIdleHook(void) {
	RtcSystemRunningIndictor();
}
