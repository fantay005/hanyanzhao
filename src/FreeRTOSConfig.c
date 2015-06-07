#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

extern void RecoveryToFactory(void);
extern void WatchdogFeed(void);

/// Malloc failed hook for FreeRTOS.
void vApplicationMallocFailedHook(void) {
	while (1) {
		printf("vApplicationMallocFailedHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 2);
	}
}

void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName ){	
	while(1){
		printf("vApplicationStackOverflowHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 2);
	}
}

/// Application idle hook for FreeRTOS.
void vApplicationIdleHook(void){
	//RecoveryToFactory();
  //	WatchdogFeed();
}
