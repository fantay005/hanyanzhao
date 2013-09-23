#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "sht1x.h"
#include "rtc.h"
#include "seven_seg_led.h"
#include "second_datetime.h"

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
	int temp;
	int humi;

	while (1) {
		vTaskDelay(configTICK_RATE_HZ / 10);
		if (RtcIsSecondInterruptOccured()) {
			DateTime dateTime;
			uint32_t second = RtcGetTime();
			SecondToDateTime(&dateTime, second);
			printf("SHT10: loop again-> %d ", second);
			printf("20%02d年%02d月%02d日 星期%d %02d:%02d:%02d\n",
				   dateTime.year, dateTime.month, dateTime.date,
				   dateTime.day,
				   dateTime.hour, dateTime.minute, dateTime.second);
			SevenSegLedSetContent(LED_INDEX_WEEK, dateTime.day);
			SevenSegLedSetContent(LED_INDEX_YEAR_H, dateTime.year / 10);
			SevenSegLedSetContent(LED_INDEX_YEAR_L, dateTime.year % 10);
			SevenSegLedSetContent(LED_INDEX_MONTH_H, dateTime.month / 10);
			SevenSegLedSetContent(LED_INDEX_MONTH_L, dateTime.month % 10);
			SevenSegLedSetContent(LED_INDEX_DATE_H, dateTime.date / 10);
			SevenSegLedSetContent(LED_INDEX_DATE_L, dateTime.date % 10);
			SevenSegLedSetContent(LED_INDEX_HOUR_H, dateTime.hour / 10);
			SevenSegLedSetContent(LED_INDEX_HOUR_L, dateTime.hour % 10);
			SevenSegLedSetContent(LED_INDEX_MINUTE_H, dateTime.minute / 10);
			SevenSegLedSetContent(LED_INDEX_MINUTE_L, dateTime.minute % 10);
			SHT10ReadTemperatureHumidity(&temp, &humi);
			temp = (temp + 5) / 10;
			if (temp > 99) {
				temp = 99;
			}
			humi = (humi + 5) / 10;
			if (humi > 99) {
				humi = 99;
			}
			printf("SHT10: temp=%d, humi=%d\n", temp, humi);
			SevenSegLedSetContent(LED_INDEX_TEMP_H, temp / 10);
			SevenSegLedSetContent(LED_INDEX_TEMP_L, temp % 10);
			SevenSegLedSetContent(LED_INDEX_HUMI_H, humi / 10);
			SevenSegLedSetContent(LED_INDEX_HUMI_L, humi % 10);
			SevenSegLedDisplay();
		}
	}
}

void SHT10TestInit(void) {
	SHT10Init();
	xTaskCreate(__ledTestTask, (signed portCHAR *) "TST", SHT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}
