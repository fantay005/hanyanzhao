#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "sht1x.h"
#include "rtc.h"
#include "seven_seg_led.h"
#include "second_datetime.h"
#include "unicode2gbk.h"
#include "softpwm_led.h"

#define SHT_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE + 64 )

#define LED_INDEX_HUMI_L 5
#define LED_INDEX_HUMI_H 6
#define LED_INDEX_TEMP_L 7
#define LED_INDEX_TEMP_H 8
#define LED_INDEX_WEEK 9
#define LED_INDEX_YEAR_H 10
#define LED_INDEX_YEAR_L 11
#define LED_INDEX_MONTH_H 12
#define LED_INDEX_MONTH_L 13
#define LED_INDEX_DATE_H 14
#define LED_INDEX_DATE_L 15
#define LED_INDEX_HOUR_H 16
#define LED_INDEX_HOUR_L 17
#define LED_INDEX_MINUTE_H 18
#define LED_INDEX_MINUTE_L 19

static void __ledTestTask(void *nouse) {
	DateTime dateTime;
	uint32_t second;	
	  
	while (1) {
		   if (!RtcWaitForSecondInterruptOccured(portMAX_DELAY)) {
			  continue;
		   }

		   second = RtcGetTime();
		//	 printf("Get Second is %d.\r", second);
	     SecondToDateTime(&dateTime, second);
		   if ((dateTime.hour == 0x00) && (dateTime.minute == 0x00) && (dateTime.second >= 0x00) && (dateTime.second <= 0x05)) {
		   		printf("Reset From Default Configuration\n");
				  vTaskDelay(configTICK_RATE_HZ * 5);
	        NVIC_SystemReset();
		   }
	}
}

void SHT10TestInit(void) {
	xTaskCreate(__ledTestTask, (signed portCHAR *) "TST", SHT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}
