#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "gsm.h"

extern void RecoveryToFactory(void);
extern void WatchdogFeed(void);

/// Malloc failed hook for FreeRTOS.
void vApplicationMallocFailedHook(void) {
#if defined (__MODEL_DEBUG__)	
	while(1){
		printf("vApplicationMallocFailedHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 3);
	}
#endif	
}

void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName ){	
#if defined (__MODEL_DEBUG__)
	while(1){
		printf("vApplicationStackOverflowHook: %s\n", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
		vTaskDelay(configTICK_RATE_HZ * 3);
	}
#endif	
}

/// Application idle hook for FreeRTOS.
void vApplicationIdleHook(void){
	//RecoveryToFactory();
#if defined (__MODEL_DEBUG__)
	
#else	
  	WatchdogFeed();
#endif	
}
