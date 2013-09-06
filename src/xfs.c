#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stdio.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "misc.h"
#include "string.h"
#include "xfs.h"


static xQueueHandle uartQueue;
static xQueueHandle speakQueue;
static int speakTimes = 3;
static int speakPause = 2;

typedef enum {
	TYPE_MSG_GB2312 = 0x00,
	TYPE_MSG_GBK = 0x01,
	TYPE_MSG_BIG5 = 0x02,
	TYPE_MSG_UCS2 = 0x03,
	TYPE_SET_SPEAKTIMES,
	TYPE_SET_SPEAKPAUSE,
} XfsMessageType;

typedef struct {
	short type;
	short len;
} SpeakMessage;

int messageGetSpeakerTimes(SpeakMessage *msg) {
	return msg->len;
}

int messageGetSpeakerPause(SpeakMessage *msg) {
	return msg->len;
}

const char *messageGetSpeakerData(SpeakMessage *msg) {
	return (const char *)&msg[1];
}

static void xfsSpeak(const char *s, int len, short type) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage) + len);
	p->type = type;
	p->len = len;
	memcpy(&p[1], s, len);
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

static void setSpeakTimesLowLevel(int times) {
	speakTimes = times;
	// write speakTimes to flash
}

static void setSpeakPauseLowLevel(int sec) {
	speakPause = sec;
	// write speakPause to flash
}

void XfsTaskSetSpeakTimes(int times) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKTIMES;
	p->len = times;
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakPause(int sec) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKPAUSE;
	p->len = sec;
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSpeakUCS2(const char *s, int len) {
	xfsSpeak(s, len, TYPE_MSG_UCS2);
}

void XfsTaskSpeakGBK(const char *s, int len) {
	xfsSpeak(s, len, TYPE_MSG_GBK);
}

void USART3_IRQHandler(void) {
	char data;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if (USART_GetITStatus(USART3, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(USART3);
	USART_SendData(USART1, data);
	USART_ClearITPendingBit(USART3, USART_IT_RXNE);
	if (pdTRUE == xQueueSendFromISR(uartQueue, &data, &xHigherPriorityTaskWoken)) {
		if (xHigherPriorityTaskWoken) {
			taskYIELD();
		}
	}
}


static int xfsSendCommand(const char *dat, int size, int timeoutTick) {
	char ret;
	portBASE_TYPE rc;
	xQueueReset(uartQueue);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, 0xFD);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, size >> 8);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, size & 0xFF);

	while (size-- > 0) {
		while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
		USART_SendData(USART3, *dat++);
	}

	rc = xQueueReceive(uartQueue, &ret, timeoutTick);
	if (rc != pdTRUE) {
		return -1;
	}
	return ret;
}

static int xfsWoken(void) {
	const char xfsCommand[] = { 0xFF };
	char ret = xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	printf("xfsWoken return %02X\n", ret);
	return 0;
}

static int xfsSetup(void) {
	const char xfsCommand[] = {  0x01, 0x01, '[', 'v', '6', ']', '[', 't', '5', ']',
								 '[', 's', '5', ']', '[', 'm', '5', '3', ']',
							  };
	char ret = xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	return 0;
}

unsigned char xfsChangePara(unsigned char type, unsigned char para) {
	char xfsCommand[7], ret;
	xfsCommand[0] = 0x01;
	xfsCommand[1] = 0x01;
	xfsCommand[2] = '[';
	xfsCommand[3] = type;
	if (type == 'm') {
		xfsCommand[4] = '5';
		xfsCommand[5] = para;
		xfsCommand[6] = ']';
	} else {
		xfsCommand[4] = para;
		xfsCommand[5] = ']';
		xfsCommand[6] = 0;
	}
	ret = xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	return 0;
}


static int xfsQueryState() {
	const char xfsCommand[] = { 0x21 };
	char ret = xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
//	printf("xfsQueryState return %02X\n", ret);
	return ret;
}

static void initHardware() {
	GPIO_InitTypeDef  GPIO_InitStructure;
	USART_InitTypeDef   USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, ENABLE);

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);					//讯飞语音模块的串口

	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART3, ENABLE);

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);					  //讯飞语音模块的RESET

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					  //讯飞语音模块的RDY

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

}

#if 0
static void xfsAlarm(void) {
	const char xfsCommand[] = { 0x01, 0x01, 's', 'o', 'u', 'n', 'd', 'b', ',' };
	xfsSendCommand(xfsCommand, sizeof(xfsCommand));
}

static void xfsBroadcast() {
}
#endif

static void xfsInitRuntime() {
	// 读取FLASH中的配置
	// speakTimes

	GPIO_ResetBits(GPIOC, GPIO_Pin_7);
	vTaskDelay(configTICK_RATE_HZ / 5);
	GPIO_SetBits(GPIOC, GPIO_Pin_7);

	xfsWoken();
	vTaskDelay(configTICK_RATE_HZ);
	xfsSetup();
	vTaskDelay(configTICK_RATE_HZ);
}

static int xfsSpeakLowLevel(const char *p, int len, char type) {
	int ret;
	portBASE_TYPE rc;
	xQueueReset(uartQueue);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, 0xFD);
	ret = len + 2;
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, ret >> 8);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, ret & 0xFF);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, 0x01);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, type);

	for (ret = 0; ret < len; ret++) {
		while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
		USART_SendData(USART3, *p++);
	}

	rc = xQueueReceive(uartQueue, &ret, configTICK_RATE_HZ);
	if (rc != pdTRUE) {
		return 0;
	}
	if (ret != 0x41) {
		return 0;
	}

	ret = 0;
	while (ret <= (len / 2)) {
		if (xfsQueryState() == 0x4F) {
			return 1;
		}
		vTaskDelay(configTICK_RATE_HZ);
		++ret;
	}
	return 0;
}

static int xfsSpeakLowLevelWithTimes(const char *p, int len, char type) {
	int i;
	for (i = 0; i < speakTimes;) {
		if (!xfsSpeakLowLevel(p, len, type)) {
			return 0;
		}
		++i;
//		if (i >= times)
//		return 1;

		vTaskDelay(configTICK_RATE_HZ * speakPause);
	}
	return 1;
}


//	TYPE_MSG_GB2312 = 0x00,
//	TYPE_MSG_GBK = 0x01,
//	TYPE_MSG_BIG5 = 0x02,
//	TYPE_MSG_UCS2 = 0x03
//	TYPE_SET_SPEAKTIMES,

#define TYPE_GB2312 0x00
#define TYPE_GBK  0x01
#define TYPE_BIG5 0x02
#define TYPE_UCS2 0x03


static void handleSpeakMessage(SpeakMessage *msg) {
	if (msg->type == TYPE_MSG_GB2312) {
		xfsSpeakLowLevelWithTimes(messageGetSpeakerData(msg), msg->len, TYPE_GB2312);
	} else if (msg->type == TYPE_MSG_GBK) {
		xfsSpeakLowLevelWithTimes(messageGetSpeakerData(msg), msg->len, TYPE_GBK);
	} else if (msg->type == TYPE_MSG_BIG5) {
		xfsSpeakLowLevelWithTimes(messageGetSpeakerData(msg), msg->len, TYPE_BIG5);
	} else if (msg->type == TYPE_MSG_UCS2) {
		xfsSpeakLowLevelWithTimes(messageGetSpeakerData(msg), msg->len, TYPE_UCS2);
	} else if (msg->type == TYPE_SET_SPEAKTIMES) {
		setSpeakTimesLowLevel(messageGetSpeakerTimes(msg));
	} else if (msg->type == TYPE_SET_SPEAKPAUSE) {
		setSpeakPauseLowLevel(messageGetSpeakerPause(msg));
	}
}

void vXfs(void *parameter) {
	portBASE_TYPE rc;
	SpeakMessage *pmsg;
	uartQueue = xQueueCreate(5, sizeof(char));		  //队列创建
	speakQueue = xQueueCreate(3, sizeof(char *));		  //队列创建

	printf("Xfs start\n");
	initHardware();
	xfsInitRuntime();
	for (;;) {
//		printf("Xfs: loop again\n");
		rc = xQueueReceive(speakQueue, &pmsg, portMAX_DELAY);
		if (rc == pdTRUE) {
			handleSpeakMessage(pmsg);
			//printf("Xfs: get reply %02X\n", dat);
//			int i;
//			for (i = 0; i < speakTimes; i++) {
//				xfsSpeakLowLevel(pmsg);
//			}
			vPortFree(pmsg);
		} else {
			//xfsSpeakGBK();
		}
	}
}
