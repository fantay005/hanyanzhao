#include <stdio.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "ledconfig.h"
#include "led_lowlevel.h"
#include "zklib.h"



#define DISPLAY_TASK_STACK_SIZE		( configMINIMAL_STACK_SIZE + 256 )

#define MSG_CMD_DISPLAY_CONTROL 0


#define	MSG_DATA_DISPLAY_CONTROL_OFF 0
#define	MSG_DATA_DISPLAY_CONTROL_ON 1

typedef struct {
	uint32_t cmd;
	union {
		void *pointData;
		uint8_t byteData;
		uint16_t halfWordData;
		uint32_t wordData;
	} data;
} DisplayTaskMessage;

static xQueueHandle __displayQueue;


void DisplayOnOff(int isOn) {
	DisplayTaskMessage msg;
	msg.cmd = MSG_CMD_DISPLAY_CONTROL;
	msg.data.byteData = isOn ? MSG_DATA_DISPLAY_CONTROL_ON : MSG_DATA_DISPLAY_CONTROL_OFF;
	xQueueSend(__displayQueue, &msg, configTICK_RATE_HZ);
}


void __handlerDisplayControl(DisplayTaskMessage *msg) {
	if (msg->data.byteData == MSG_DATA_DISPLAY_CONTROL_OFF) {
		// display off
		LedScanOnOff(0);
	} else if (msg->data.byteData == MSG_DATA_DISPLAY_CONTROL_ON) {
		// display on
		LedScanOnOff(1);
	} else {
		// unknow
	}
}

typedef void (*MessageHandlerFunc)(DisplayTaskMessage *);
typedef void (*MessageDestroyFunc)(DisplayTaskMessage *);
static const struct {
	uint32_t cmd;
	MessageHandlerFunc handlerFunc;
	MessageDestroyFunc destroyFunc;
} __messageHandlerFunctions[] = {
	{ MSG_CMD_DISPLAY_CONTROL, __handlerDisplayControl, NULL},
};

void __leftToRight() {
	int x;
	for (x = 0; x <= LED_DOT_XEND; ++x) {
		LedDisplayToScan(x, 0, x, LED_DOT_YEND);
		vTaskDelay(configTICK_RATE_HZ / 50);
	}
}

void __upperToLower() {
	int y;
	for (y = 0; y <= LED_DOT_YEND; ++y) {
		LedDisplayToScan(0, y, LED_DOT_XEND, y);
		vTaskDelay(configTICK_RATE_HZ / 20);
	}
}

void __rightToLeft() {
	int x;
	for (x = LED_DOT_XEND; x >= 0; --x) {
		LedDisplayToScan(x, 0, x, LED_DOT_YEND);
		vTaskDelay(configTICK_RATE_HZ / 50);
	}
}

void __lowerToUpper() {
	int y;
	for (y = LED_DOT_YEND; y >= 0; --y) {
		LedDisplayToScan(0, y, LED_DOT_XEND, y);
		vTaskDelay(configTICK_RATE_HZ / 20);
	}
}


void DisplayTask(void *helloString) {
	portBASE_TYPE rc;
	DisplayTaskMessage msg;

	printf("DisplayTask: start-> %s\n", (const char *)helloString);
	__displayQueue = xQueueCreate(5, sizeof(DisplayTaskMessage));
	LedDisplayGB2312String32(0, 0, (const uint8_t *)helloString);
	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);
	LedScanOnOff(1);
	while (1) {
		rc = xQueueReceive(__displayQueue, &msg, configTICK_RATE_HZ * 2);
		if (rc == pdTRUE) {
			int i;
			for (i = 0; i < ARRAY_MEMBER_NUMBER(__messageHandlerFunctions); ++i) {
				if (__messageHandlerFunctions[i].cmd == msg.cmd) {
					__messageHandlerFunctions[i].handlerFunc(&msg);
					if (__messageHandlerFunctions[i].destroyFunc != NULL) {
						__messageHandlerFunctions[i].destroyFunc(&msg);
					}
					break;
				}
			}
		} else {
			static unsigned char a = 0;
			static void (*func[])(void) = {
				__leftToRight, __upperToLower, __rightToLeft, __lowerToUpper
			};

			printf("DisplayTask: %d\n", a);
			LedScanClear(0, 0, LED_DOT_XEND, LED_DOT_YEND);
			vTaskDelay(configTICK_RATE_HZ / 2);
			func[a]();
			if (a >= 3) {
				a = 0;
			} else {
				++a;
			}

		}
	}
}

void DisplayInit(void) {
	LedScanInit();
	xTaskCreate(DisplayTask, (signed portCHAR *) "DSP", DISPLAY_TASK_STACK_SIZE, "ÖÐ¿Æ½ð?!", tskIDLE_PRIORITY + 10, NULL);
}

