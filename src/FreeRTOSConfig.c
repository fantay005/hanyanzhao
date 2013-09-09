#include "FreeRTOS.h"

void vApplicationMallocFailedHook(void) {
	volatile int exit = 0;
	while(! exit) {	
		printf("vApplicationMallocFailedHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 2);
	}
}