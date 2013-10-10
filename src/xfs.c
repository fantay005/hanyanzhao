#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "misc.h"
#include "xfs.h"
#include "norflash.h"

static xQueueHandle uartQueue;
static xQueueHandle speakQueue;

#define XFS_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE + 256 )

static struct {
	unsigned char speakTimes;
	unsigned char speakPause;
	unsigned char speakVolume;
	unsigned char speakType;
	unsigned char speakSpeed;
	unsigned char speakTone;
} speakParam = {3, 2, '5', '3', '5', '5'};

static inline void storeSpeakParam(void) {
	NorFlashWrite(XFS_PARAM_STORE_ADDR, (const short *)&speakParam, sizeof(speakParam));
}

void restorSpeakParam(void) {
	NorFlashRead(XFS_PARAM_STORE_ADDR, (short *)&speakParam, sizeof(speakParam));
	if (speakParam.speakTimes > 100) {
		speakParam.speakTimes = 3;
	}
	if (speakParam.speakPause > 100) {
		speakParam.speakPause = 2;
	}
	if (speakParam.speakVolume > 100) {
		speakParam.speakVolume = '5';
	}
	if (speakParam.speakType > 100) {
		speakParam.speakType = '3';
	}
	if (speakParam.speakSpeed > 100) {
		speakParam.speakSpeed = '5';
	}
	if (speakParam.speakTone > 100) {
		speakParam.speakTone = '5';
	}
}

typedef enum {
	TYPE_MSG_GB2312 = 0x00,
	TYPE_MSG_GBK = 0x01,
	TYPE_MSG_BIG5 = 0x02,
	TYPE_MSG_UCS2 = 0x03,
	TYPE_SET_SPEAKTIMES,
	TYPE_SET_SPEAKPAUSE,
	TYPE_SET_SPEAKVOLUME = 'v',
	TYPE_SET_SPEAKTYPE = 'm',
	TYPE_SET_SPEAKTONE = 't',
	TYPE_SET_SPEAKSPEED = 's',
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

int messageGetSpeakerTone(SpeakMessage *msg) {
	return msg->len;
}

int messageGetSpeakerVolume(SpeakMessage *msg) {
	return msg->len;
}

int messageGetSpeakerType(SpeakMessage *msg) {
	return msg->len;
}

int messageGetSpeakerSpeed(SpeakMessage *msg) {
	return msg->len;
}

const char *messageGetSpeakerData(SpeakMessage *msg) {
	return (const char *)&msg[1];
}

unsigned char xfsChangePara(unsigned char type, unsigned char para) {
	char xfsCommand[7];
	xfsCommand[0] = 0x01;
	xfsCommand[1] = 0x01;
	xfsCommand[2] = '[';
	xfsCommand[3] = type;
	if (type == 'm') {
		xfsCommand[4] = '5';
		xfsCommand[5] = para;
		xfsCommand[6] = ']';
		if (para == 0x33) {
			xfsCommand[4] =	0x33;
			xfsCommand[5] =	']';
			xfsCommand[6] =	0;
		}
	} else {
		xfsCommand[4] = para;
		xfsCommand[5] = ']';
		xfsCommand[6] = 0;
	}
	xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	return 0;
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
	speakParam.speakTimes = times;
	storeSpeakParam();
}

static void setSpeakPauseLowLevel(int sec) {
	speakParam.speakPause = sec;
	storeSpeakParam();
}

static void setSpeakTypeLowLevel(char Type) {
	speakParam.speakType = Type;
	storeSpeakParam();
}

static void setSpeakVolumeLowLevel(char Vol) {
	speakParam.speakVolume = Vol;
	storeSpeakParam();
}

static void setSpeakSpeedLowLevel(char Speed) {
	speakParam.speakSpeed = Speed;
	storeSpeakParam();
}

static void setSpeakToneLowLevel(char Tone) {
	speakParam.speakTone = Tone;
	storeSpeakParam();
}

void XfsTaskSetSpeakTimes(int times) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKTIMES;
	p->len = times;
	xfsChangePara(p->type, p->len);
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakPause(int sec) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKPAUSE;
	p->len = sec;
	xfsChangePara(p->type, p->len);
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakVolume(char Vol) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKVOLUME;
	p->len = Vol;
	xfsChangePara(p->type, p->len);
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakType(char Type) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKTYPE;
	p->len = Type;
	xfsChangePara(p->type, p->len);
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakTone(char Tone) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKTONE;
	p->len = Tone;
	xfsChangePara(p->type, p->len);
	if (pdTRUE != xQueueSend(speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakSpeed(char Speed) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage));
	p->type = TYPE_SET_SPEAKTONE;
	p->len = Speed;
	xfsChangePara(p->type, p->len);
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
	char xfsCommand[] = {0x01, 0x01, '[', '5', '5', ']', '[', 't', '5', ']',
						 '[', 's', '5', ']', '[', 'm', '3', ']'
						};
	xfsCommand[4] = speakParam.speakVolume;
	xfsCommand[8] = speakParam.speakTone;
	xfsCommand[12] = speakParam.speakSpeed;
	xfsCommand[16] = speakParam.speakType;
	xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
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
	
#if defined(__SPEAKER__)
	GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, ENABLE);

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);					//Ñ¶·ÉÓïÒôÄ£¿éµÄ´®¿Ú

#elif(__LED__)
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					//Ñ¶·ÉÓïÒôÄ£¿éµÄ´®¿Ú
#endif

#if defined(__LED_LIXIN__) && (__LED_LIXIN__!=0)
	GPIO_ResetBits(GPIOA, GPIO_Pin_6);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
#endif

	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART3, ENABLE);

#if defined(__SPEAKER__)
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);					  //Ñ¶·ÉÓïÒôÄ£¿éµÄRESET

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					  //Ñ¶·ÉÓïÒôÄ£¿éµÄRDY
#endif

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

static void xfsInitRuntime() {
#if defined (__SPEAKER__)
	GPIO_ResetBits(GPIOC, GPIO_Pin_7);
	vTaskDelay(configTICK_RATE_HZ / 5);
	GPIO_SetBits(GPIOC, GPIO_Pin_7);
#elif defined (__LED__)
	vTaskDelay(configTICK_RATE_HZ / 5);
#endif
	xfsWoken();
	vTaskDelay(configTICK_RATE_HZ);
	xfsSetup();
	vTaskDelay(configTICK_RATE_HZ);
}

#define TYPE_GB2312 0x00
#define TYPE_GBK  0x01
#define TYPE_BIG5 0x02
#define TYPE_UCS2 0x03

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
	while (ret <= len) {
		if (xfsQueryState() == 0x4F) {
			return 1;
		}
		vTaskDelay(configTICK_RATE_HZ / 2);
		++ret;
	}
	return 0;
}

static int xfsSpeakLowLevelWithTimes(const char *p, int len, char type) {
	int i;

#if defined(__LED_LIXIN__) && (__LED_LIXIN__!=0)
	// ÉùÒô--¡·Ñ¶·É
	GPIO_SetBits(GPIOA, GPIO_Pin_6);
#endif
	for (i = 0; i < speakParam.speakTimes; ++i) {
		if (!xfsSpeakLowLevel(p, len, type)) {
#if defined(__LED_LIXIN__) && (__LED_LIXIN__!=0)
		// ÉùÒô--¡·
		GPIO_ResetBits(GPIOA, GPIO_Pin_6);
#endif 
			return 0;
		}
		vTaskDelay(configTICK_RATE_HZ * speakParam.speakPause);
	}
#if defined(__LED_LIXIN__) && (__LED_LIXIN__!=0)
		// ÉùÒô--¡·
		GPIO_ResetBits(GPIOA, GPIO_Pin_6); 
#endif
	return 1;
}

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
	} else if (msg->type == TYPE_SET_SPEAKVOLUME) {
		setSpeakVolumeLowLevel(messageGetSpeakerVolume(msg));
	} else if (msg->type == TYPE_SET_SPEAKTONE) {
		setSpeakToneLowLevel(messageGetSpeakerTone(msg));
	} else if (msg->type == TYPE_SET_SPEAKTYPE) {
		setSpeakTypeLowLevel(messageGetSpeakerType(msg));
	} else if (msg->type == TYPE_SET_SPEAKSPEED) {
		setSpeakSpeedLowLevel(messageGetSpeakerType(msg));
	}
}

void __xfsTask(void *parameter) {
	portBASE_TYPE rc;
	SpeakMessage *pmsg;
	uartQueue = xQueueCreate(5, sizeof(char));
	speakQueue = xQueueCreate(3, sizeof(char *));

	printf("Xfs start\n");
	restorSpeakParam();
	xfsInitRuntime();
//	DisplayInit();
	for (;;) {
		printf("Xfs: loop again\n");
		rc = xQueueReceive(speakQueue, &pmsg, portMAX_DELAY);
		if (rc == pdTRUE) {
			restorSpeakParam();
			handleSpeakMessage(pmsg);
			vPortFree(pmsg);
		} else {
			// xfsSpeakGBK();
		}
	}
}

void XfsInit(void) {
	initHardware();
	xTaskCreate(__xfsTask, (signed portCHAR *) "XFS", XFS_TASK_STACK_SIZE, (void *)'2', tskIDLE_PRIORITY + 2, NULL);
}
