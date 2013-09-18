#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "sht1x.h"

#define SHT_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE + 64 )

static void __sht10Task(void *nouse) {
	int temp;
	int humi;

	while (1) {
		printf("SHT10: loop again\n");
		SHT10ReadTemperatureHumidity(&temp, &humi);
		printf("SHT10: temp=%d, humi=%d\n", temp, humi);
		vTaskDelay(configTICK_RATE_HZ * 5);
	}
}

void SHT10TestInit(void) {
	SHT10Init();
	xTaskCreate(__sht10Task, (signed portCHAR *) "SHT", SHT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}
