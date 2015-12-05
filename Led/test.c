#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "rtc.h"
#include "second_datetime.h"
#include "sms.h"
#include "gsm.h"

#define SHT_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE)


static void __TimeTask(void *nouse) {
	DateTime dateTime;
	uint32_t second;
	 
	while (1) {
		 if (!RtcWaitForSecondInterruptOccured(portMAX_DELAY)) {
			continue;
		 }	 
		 
		 second = RtcGetTime();
		 SecondToDateTime(&dateTime, second);

		if((dateTime.hour == 0x0C) && (dateTime.minute == 0x1E) && (dateTime.second == 0x00)){		
			NVIC_SystemReset();			
		} 
		
		if((dateTime.date == 0x05) && (dateTime.hour == 0x0A) && (dateTime.minute == 0x00) && (dateTime.second == 0x00)){
			vTaskDelay(configTICK_RATE_HZ);
			
			
		}
	}
}

void TimePlanInit(void) {
	xTaskCreate(__TimeTask, (signed portCHAR *) "TEST", SHT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
}


