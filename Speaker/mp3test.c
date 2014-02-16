#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "music.h"
#include "mp3.h"

#define mp3_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE + 64 )
#define tu 1024

static void __mp3TestTask(void *nouse) {

	   uint32_t i;
	   int m, n;
	   m = sizeof(music) / tu;
	   n = sizeof(music) % tu;
	   while (1) {
		   for(i = 0; i < m*tu; i+=tu){
		   	  VS1003_Play((const unsigned char*)&music[i], tu);
		   }
		   VS1003_Play((const unsigned char*)&music[m*tu], n);
		   vTaskDelay(2 * configTICK_RATE_HZ);
	 }
}

void mp3TestInit(void) {

	xTaskCreate(__mp3TestTask, (signed portCHAR *) "mp3", mp3_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}
