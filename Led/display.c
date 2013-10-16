#ifdef __LED__
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "ledconfig.h"
#include "led_lowlevel.h"
#include "zklib.h"
#include "display.h"
#include "norflash.h"
#include "xfs.h"
#include "unicode2gbk.h"
#include "font_dot_array.h"

#define DISPLAY_TASK_STACK_SIZE		( configMINIMAL_STACK_SIZE + 256 )

#define MSG_CMD_DISPLAY_CONTROL 0
#define MSG_CMD_DISPLAY_MESSAGE	1
#define MSG_CMD_DISPLAY_MESSAGE_RED	1
#define MSG_CMD_DISPLAY_MESSAGE_GREEN	2
#define MSG_CMD_DISPLAY_MESSAGE_YELLOW	3


#define	MSG_DATA_DISPLAY_CONTROL_OFF 0
#define	MSG_DATA_DISPLAY_CONTROL_ON 1

#if defined(__LED_HUAIBEI__)
static const char *host = "安徽气象欢迎您！";
static const char *assistant = "淮北气象三农服务";
#endif


#if defined(__LED_LIXIN__)
static const char *host = "安徽气象欢迎您！";
#endif

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


static char __displayMessageColor = 1;
static const uint8_t *__displayMessage = NULL;
static const uint8_t *__displayCurrentPoint = NULL;

void MessDisplay(char *message) {
	char *p = pvPortMalloc(strlen(message) + 1);
	DisplayTaskMessage msg;
	strcpy(p, message);
	msg.cmd = MSG_CMD_DISPLAY_MESSAGE;
	msg.data.pointData = p;

	if (pdTRUE != xQueueSend(__displayQueue, &msg, configTICK_RATE_HZ)) {
		vPortFree(p);
	}
}

void DisplayMessageRed(const char *message) {
	char *p = pvPortMalloc(strlen(message) + 1);
	DisplayTaskMessage msg;
	strcpy(p, message);
	msg.cmd = MSG_CMD_DISPLAY_MESSAGE_RED;
	msg.data.pointData = p;

	if (pdTRUE != xQueueSend(__displayQueue, &msg, configTICK_RATE_HZ)) {
		vPortFree(p);
	}
}

void DisplayMessageGreen(const char *message) {
	char *p = pvPortMalloc(strlen(message) + 1);
	DisplayTaskMessage msg;
	strcpy(p, message);
	msg.cmd = MSG_CMD_DISPLAY_MESSAGE_GREEN;
	msg.data.pointData = p;

	if (pdTRUE != xQueueSend(__displayQueue, &msg, configTICK_RATE_HZ)) {
		vPortFree(p);
	}
}

void DisplayMessageYELLOW(const char *message) {
	char *p = pvPortMalloc(strlen(message) + 1);
	DisplayTaskMessage msg;
	strcpy(p, message);
	msg.cmd = MSG_CMD_DISPLAY_MESSAGE_YELLOW;
	msg.data.pointData = p;

	if (pdTRUE != xQueueSend(__displayQueue, &msg, configTICK_RATE_HZ)) {
		vPortFree(p);
	}
}


void DisplayOnOff(int isOn) {
	DisplayTaskMessage msg;
	msg.cmd = MSG_CMD_DISPLAY_CONTROL;
	msg.data.byteData = isOn ? MSG_DATA_DISPLAY_CONTROL_ON : MSG_DATA_DISPLAY_CONTROL_OFF;
	xQueueSend(__displayQueue, &msg, configTICK_RATE_HZ);
}

#if defined(__LED_LIXIN__)
void __displayMessageLowlevel(void) {
	const uint8_t *tmp;
	if (__displayMessage == NULL) {
		return;
	}
	if (__displayCurrentPoint == NULL) {
		__displayCurrentPoint = __displayMessage;
	}
	LedDisplayClear(0, 0, LED_DOT_XEND, LED_DOT_HEIGHT / 2 - 1);
	LedDisplayClear(0, LED_DOT_HEIGHT / 2, LED_DOT_XEND, LED_DOT_HEIGHT - 1);
	if (__displayMessageColor & 1) {
		tmp = LedDisplayGB2312String32(0, 0, LED_DOT_WIDTH / 8, 32, __displayCurrentPoint);
	}

	if (__displayMessageColor & 2) {
		tmp = LedDisplayGB2312String32(0, 32, LED_DOT_WIDTH / 8, 64, __displayCurrentPoint);
	}
	__displayCurrentPoint = tmp;
	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);

}
#endif

#if defined(__LED_HUAIBEI__)
void __displayMessageLowlevel(void) {
	const uint8_t *tmp;
	if (__displayMessage == NULL) {
		return;
	}
	if (__displayCurrentPoint == NULL) {
		__displayCurrentPoint = __displayMessage;
	}
	LedDisplayClear(0, 0, LED_DOT_XEND, LED_DOT_HEIGHT / 2 - 1);
	LedDisplayClear(0, LED_DOT_HEIGHT / 2, LED_DOT_XEND, LED_DOT_HEIGHT - 1);
	if (__displayMessageColor & 1) {
		tmp = LedDisplayGB2312String16(0, 0, __displayCurrentPoint);
	}

//	if (__displayMessageColor & 2) {
//		tmp = LedDisplayGB2312String16(0, 4, __displayCurrentPoint);
//	}
	__displayCurrentPoint = tmp;
	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);

}
#endif


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

void __handlerDisplayMessage(DisplayTaskMessage *msg) {
	if (__displayMessage) {
		vPortFree((void *)__displayMessage);
	}
	__displayCurrentPoint = NULL;
	__displayMessage = msg->data.pointData;
	__displayMessageLowlevel();
}


void __handlerDisplayMessageRed(DisplayTaskMessage *msg) {
	if (__displayMessage) {
		vPortFree((void *)__displayMessage);
	}
	__displayCurrentPoint = NULL;
	__displayMessage = msg->data.pointData;
	__displayMessageColor = 1;
	__displayMessageLowlevel();
}

void __handlerDisplayMessageGreen(DisplayTaskMessage *msg) {
	if (__displayMessage) {
		vPortFree((void *)__displayMessage);
	}
	__displayCurrentPoint = NULL;
	__displayMessage = msg->data.pointData;
	__displayMessageColor = 2;
	__displayMessageLowlevel();
}

void __handlerDisplayMessageYellow(DisplayTaskMessage *msg) {
	if (__displayMessage) {
		vPortFree((void *)__displayMessage);
	}
	__displayCurrentPoint = NULL;
	__displayMessage = msg->data.pointData;
	__displayMessageColor = 3;
	__displayMessageLowlevel();
}

void __destroyDisplayMessage(DisplayTaskMessage *msg) {
	vPortFree(msg->data.pointData);
}

typedef void (*MessageHandlerFunc)(DisplayTaskMessage *);
typedef void (*MessageDestroyFunc)(DisplayTaskMessage *);
static const struct {
	uint32_t cmd;
	MessageHandlerFunc handlerFunc;
	MessageDestroyFunc destroyFunc;
} __messageHandlerFunctions[] = {
	{ MSG_CMD_DISPLAY_MESSAGE, __handlerDisplayMessage, NULL },

#if defined(__LED_LIXIN__)
	{ MSG_CMD_DISPLAY_MESSAGE_RED, __handlerDisplayMessageRed, NULL },
	{ MSG_CMD_DISPLAY_MESSAGE_GREEN, __handlerDisplayMessageGreen, NULL },
	{ MSG_CMD_DISPLAY_MESSAGE_YELLOW, __handlerDisplayMessageYellow, NULL },
#endif

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
#if defined(__LED_LIXIN__)
void DisplayClear(void) {
	char clear[144];
	int i;
	for (i = 0; i < 144; i++) {
		clear[i] = ' ';
	}
	LedDisplayGB2312String32(0, 0, LED_DOT_WIDTH / 8, LED_DOT_YEND-1, (const uint8_t *)clear);
	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);
}
#endif

void DisplayClear(void) {
	char clear[144];
	int i;
	for (i = 0; i < 144; i++) {
		clear[i] = ' ';
	}
	LedDisplayGB2312String16(0, 0, (const uint8_t *)clear);
	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);
}

void Display2Clear(void) {
	char clear[24];
	int i;
	for (i = 0; i < 24; i++) {
		clear[i] = ' ';
	}
	LedDisplayGB2312String162(0, 0, (const uint8_t *)clear);
	LedDisplayToScan2(2*4, 0, LED_DOT_XEND, LED_DOT_YEND);
}

#if defined(__LED_LIXIN__)
void DisplayTask(void *helloString) {
	portBASE_TYPE rc;
	DisplayTaskMessage msg;

	printf("DisplayTask: start-> %s\n", (const char *)helloString);
	__displayQueue = xQueueCreate(5, sizeof(DisplayTaskMessage));
	{
		const char *p = (const char *)(Bank1_NOR2_ADDR + SMS1_PARAM_STORE_ADDR);
		if (isGB2312Start(p[0]) && isGB2312Start(p[1])) {
			host = p;
		} else if (isAsciiStart(p[0])) {
			host = p;
		}
		p = (const char *)(Bank1_NOR2_ADDR + SMS2_PARAM_STORE_ADDR);
		if (isGB2312Start(p[0]) && isGB2312Start(p[1])) {
			assistant = p;
		} else if (isAsciiStart(p[0])) {
			assistant = p;
		}
	}

	LedDisplayGB2312String32(288 / 8, 0, LED_DOT_WIDTH / 8, 32, host);
	LedDisplayGB2312String32(288 / 8, 32, LED_DOT_WIDTH / 8, 64, host);
	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);
	LedScanOnOff(1);
	while (1) {
		rc = xQueueReceive(__displayQueue, &msg, configTICK_RATE_HZ * 5);
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
			__displayMessageLowlevel();
		}
	}
}

#endif

#if defined(__LED_HUAIBEI__)
void DisplayTask(void *helloString) {
	portBASE_TYPE rc;
	DisplayTaskMessage msg;

	printf("DisplayTask: start-> %s\n", (const char *)helloString);
	__displayQueue = xQueueCreate(5, sizeof(DisplayTaskMessage));
	{
		const char *p = (const char *)(Bank1_NOR2_ADDR + SMS1_PARAM_STORE_ADDR);
		if (isGB2312Start(p[0]) && isGB2312Start(p[1])) {
			host = p;
		} else if (isAsciiStart(p[0])) {
			host = p;
		}
		p = (const char *)(Bank1_NOR2_ADDR + SMS2_PARAM_STORE_ADDR);
		if (isGB2312Start(p[0]) && isGB2312Start(p[1])) {
			assistant = p;
		} else if (isAsciiStart(p[0])) {
			assistant = p;
		}
	}
	LedDisplayGB2312String16(0, 0, host);
	LedDisplayGB2312String162(8, 0, assistant);
	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);
	LedDisplayToScan2(2 * 4, 0, LED_DOT_XEND, 15);
	LedScanOnOff(1);
	while (1) {
		rc = xQueueReceive(__displayQueue, &msg, configTICK_RATE_HZ * 5);
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
			__displayMessageLowlevel();
		}
	}
}

#endif


void DisplayInit(void) {
	LedScanInit();
	xTaskCreate(DisplayTask, (signed portCHAR *) "DISPLAY", DISPLAY_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 10, NULL);
}

#endif

