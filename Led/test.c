#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "sht1x.h"
#include "seven_seg_led.h"

#define SHT_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE + 64 )

#define LED_INDEX_WEEK 9
#define LED_INDEX_TEMP_H 8
#define LED_INDEX_TEMP_L 7
#define LED_INDEX_HUMI_H 6
#define LED_INDEX_HUMI_L 5
#define LED_INDEX_YEAR_H 10
#define LED_INDEX_YEAR_L 11
#define LED_INDEX_MONTH_H 12
#define LED_INDEX_MONTH_L 13
#define LED_INDEX_DATE_H 14
#define LED_INDEX_DATE_L 15
#define LED_INDEX_HOUR_H 16
#define LED_INDEX_HOUR_L 17
#define LED_INDEX_MINUTE_H 16
#define LED_INDEX_MINUTE_L 17

static void __ledTestTask(void *nouse) {
	int temp;
	int humi;
// 	int i = 0;
// 	int j;
	{
	int i;
	for (i = 0; i < SEVEN_SEG_LED_NUM; ++i) {
		SevenSegLedSetContent(i, i%10);
	}
	SevenSegLedDisplay();	
	vTaskDelay(configTICK_RATE_HZ * 10);
	}

	while (1) {
		printf("SHT10: loop again:\n");
		SHT10ReadTemperatureHumidity(&temp, &humi);
		temp += 5;
		if (temp > 999) temp = 999;
		humi += 5;
		if (humi > 999) humi = 999;
		printf("SHT10: temp=%d, humi=%d\n", temp, humi);
		SevenSegLedSetContent(LED_INDEX_TEMP_H, temp/100);
		SevenSegLedSetContent(LED_INDEX_TEMP_L, (temp%100)/10);
		SevenSegLedSetContent(LED_INDEX_HUMI_H, humi/100);
		SevenSegLedSetContent(LED_INDEX_HUMI_L, (humi%100)/10);
		SevenSegLedDisplay();	
		
// 		if (++i == 10) {
// 			i = 0;
// 		}
// 		
// 		for (j = 0; j < SEVEN_SEG_LED_NUM; ++j) {
// 			SevenSegLedSetContent(j, i);
// 		}
// 		SevenSegLedDisplay();		
	}
}

void SHT10TestInit(void) {
	SHT10Init();
	xTaskCreate(__ledTestTask, (signed portCHAR *) "TST", SHT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}
