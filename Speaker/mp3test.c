#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "music.h"
#include "mp3.h"

#define mp3_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE + 64 )

static void __mp3TestTask(void *nouse) {
	   uint32_t i;
	   while (1) {
	   for(i = 0; i < sizeof(music); i+=512){
	   	  VS1003_Play((const unsigned char*)&music[i], 512);
	   }
	   vTaskDelay(5 * configTICK_RATE_HZ);
	 }
}

void mp3TestInit(void) {

	xTaskCreate(__mp3TestTask, (signed portCHAR *) "mp3", mp3_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}
