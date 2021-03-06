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
#include "soundcontrol.h"

#define	SMS_PIN  GPIO_Pin_14

static xQueueHandle __uartQueue;
static xQueueHandle __speakQueue;

static XFSspeakParam  speakParam = {3, 5, '9', '3', '5', '5'};

#define XFS_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE + 256 )

static inline void __storeSpeakParam(void) {
	NorFlashWrite(XFS_PARAM_STORE_ADDR, (const short *)&speakParam.speakTimes, sizeof(speakParam));
}

static void __restorSpeakParam(void) {
	NorFlashRead(XFS_PARAM_STORE_ADDR, (short *)&speakParam.speakTimes, sizeof(speakParam));
	if (speakParam.speakTimes > 100) {
		speakParam.speakTimes = 3;
	}
	if (speakParam.speakPause > 100) {
		speakParam.speakPause = 5;
	}
	if (speakParam.speakVolume > 100) {
		speakParam.speakVolume = '8';
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
	TYPE_MSG_GB2312,
	TYPE_MSG_GBK,
	TYPE_MSG_BIG5,
	TYPE_MSG_UCS2,
	TYPE_SET_SPEAKTIMES,
	TYPE_SET_SPEAKPAUSE,
	TYPE_SET_SPEAKVOLUME,
	TYPE_SET_SPEAKTYPE,
	TYPE_SET_SPEAKTONE,
	TYPE_SET_SPEAKSPEED,
} XfsMessageType;

typedef struct {
	short type;
	short len;
} XfsTaskMessage;

static int __messageGetSpeakerTimes(XfsTaskMessage *msg) {
	return msg->len;
}

static int __messageGetSpeakerPause(XfsTaskMessage *msg) {
	return msg->len;
}

static int __messageGetSpeakerTone(XfsTaskMessage *msg) {
	return msg->len;
}

static int __messageGetSpeakerVolume(XfsTaskMessage *msg) {
	return msg->len;
}

static int __messageGetSpeakerType(XfsTaskMessage *msg) {
	return msg->len;
}

//static int __messageGetSpeakerSpeed(XfsTaskMessage *msg) {
//	return msg->len;
//}

static const char *__messageGetSpeakerData(XfsTaskMessage *msg) {
	return (const char *)&msg[1];
}

static void __xfsSendByte(char c) {
	USART_SendData(USART3, c);
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
}

static int __xfsSendCommand(const char *dat, int size, int timeoutTick) {
	char ret;
	portBASE_TYPE rc;
	xQueueReset(__uartQueue);
	__xfsSendByte(0xFD);
	__xfsSendByte(size >> 8);
	__xfsSendByte(size & 0xFF);

	while (size-- > 0) {
		__xfsSendByte(*dat++);
	}

	rc = xQueueReceive(__uartQueue, &ret, timeoutTick);
	if (rc != pdTRUE) {
		return -1;
	}
	return ret;
}

void recover(void){
	__xfsSendByte(0xFD);
	__xfsSendByte(0x00);
	__xfsSendByte(0x06);
  __xfsSendByte(0x01);
  __xfsSendByte(TYPE_MSG_UCS2);
	__xfsSendByte('[');
	__xfsSendByte('v');
	__xfsSendByte(speakParam.speakVolume);
	__xfsSendByte(']');
}

void SMS_Prompt(char *p, int len) {
	char size;
	int i;
	size = len + 6;
	__xfsSendByte(0xFD);
	__xfsSendByte(size >> 8);
	__xfsSendByte(size & 0xFF);
  __xfsSendByte(0x01);
  __xfsSendByte(TYPE_MSG_UCS2);
	__xfsSendByte('[');
	__xfsSendByte('v');
	__xfsSendByte('1');
	__xfsSendByte(']');
	

	for (i = 0; i < len; i++) {
		USART_SendData(USART3, *p++);
		while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	}
	
	recover();
}

static unsigned char __xfsChangePara(unsigned char type, unsigned char para) {
	int len;
	char xfsCommand[7];

	xfsCommand[0] = 0x01;
	xfsCommand[1] = 0x01;
	xfsCommand[2] = '[';
	xfsCommand[3] = type;
	if (type == 'm') {
		if (para == '3') {
			xfsCommand[4] =	'3';
			xfsCommand[5] =	']';
			len = 6;
		} else {
			xfsCommand[4] = '5';
			xfsCommand[5] = para;
			xfsCommand[6] = ']';
			len = 7;
		}
	} else {
		xfsCommand[4] = para;
		xfsCommand[5] = ']';
		len = 6;
	}
	__xfsSendCommand(xfsCommand, len, configTICK_RATE_HZ);
	return 0;
}

static void __xfsSpeak(const char *s, int len, short type) {
	XfsTaskMessage *p = (XfsTaskMessage *)pvPortMalloc(sizeof(XfsTaskMessage) + len);
	p->type = type;
	p->len = len;
	memcpy(&p[1], s, len);
	if (pdTRUE != xQueueSend(__speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

static void __setSpeakTimesLowLevel(int times) {
	speakParam.speakTimes = times;
	__storeSpeakParam();
}

static void __setSpeakPauseLowLevel(int sec) {
	speakParam.speakPause = sec;
	__storeSpeakParam();
}

static void __setSpeakTypeLowLevel(char type) {
	speakParam.speakType = type;
	__storeSpeakParam();
	__xfsChangePara('m', type);
}

static void __setSpeakVolumeLowLevel(char vol) {
	speakParam.speakVolume = vol;
	__storeSpeakParam();
	__xfsChangePara('v', vol);
}

static void __setSpeakSpeedLowLevel(char speed) {
	speakParam.speakSpeed = speed;
	__storeSpeakParam();
	__xfsChangePara('s', speed);
}

static void __setSpeakToneLowLevel(char tone) {
	speakParam.speakTone = tone;
	__storeSpeakParam();
	__xfsChangePara('t', tone);
}


static int __xfsWoken(void) {
	const char xfsCommand[] = { 0xFF };
	char ret;
  do{
    ret = __xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	  printf("xfsWoken return %02X\n", ret);
	}while(ret != 0x4A);
	return 0;
}

static int __xfsSetup(void) {
	char xfsCommand[] = {0x01, 0x01, '[', 'v', '5', ']', '[', 't', '5', ']',
						 '[', 's', '5', ']', '[', 'm', '3', ']'
						};
	xfsCommand[4] = speakParam.speakVolume;
	xfsCommand[8] = speakParam.speakTone;
	xfsCommand[12] = speakParam.speakSpeed;
	xfsCommand[16] = speakParam.speakType;
	__xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	return 0;
}

int __xfsQueryState() {
	const char xfsCommand[] = { 0x21 };
	char ret = __xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
//	printf("xfsQueryState return %02X\n", ret);
	return ret;
}

static void __initHardware() {
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
	GPIO_Init(GPIOC, &GPIO_InitStructure);					//Ѷ������ģ��Ĵ���

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
	GPIO_Init(GPIOC, &GPIO_InitStructure);					  //Ѷ������ģ���RESET

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					  //Ѷ������ģ���RDY
#endif

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

static void __xfsInitRuntime() {

	GPIO_ResetBits(GPIOC, GPIO_Pin_7);
	vTaskDelay(configTICK_RATE_HZ / 5);
	GPIO_SetBits(GPIOC, GPIO_Pin_7);
	
	__xfsWoken();
	vTaskDelay(configTICK_RATE_HZ);
	__xfsSetup();
	vTaskDelay(configTICK_RATE_HZ);
}

#define TYPE_GB2312 0x00
#define TYPE_GBK  0x01
#define TYPE_BIG5 0x02
#define TYPE_UCS2 0x03

#if defined (__SPEAKER_V1__)||(__SPEAKER_V2__)

static int __xfsSpeakLowLevel(const char *p, int len, char type) {
	int ret;
	portBASE_TYPE rc;
	xQueueReset(__uartQueue);

	__xfsSendByte(0xFD);
	ret = len + 2;
	__xfsSendByte(ret >> 8);
	__xfsSendByte(ret & 0xFF);
	__xfsSendByte(0x01);
	__xfsSendByte(type);

	for (ret = 0; ret < len; ret++) {
		__xfsSendByte(*p++);
	}

	rc = xQueueReceive(__uartQueue, &ret, configTICK_RATE_HZ * 5);
//	vTaskDelay(configTICK_RATE_HZ );
	if (rc != pdTRUE) {
		return 0;
	}
	if (ret != 0x41) {
		return 0;
	}
	ret = 0;
	while (ret <= len) {
		if (__xfsQueryState() == 0x4F) {
			return 1;
		}
		vTaskDelay(configTICK_RATE_HZ / 2);
		++ret;
	}
	return 1;
}

#endif

#if defined(__SPEAKER_V3__)
static int __xfsSpeakLowLevel(const char *p, int len, char type) {
	int ret;
	portBASE_TYPE rc;
	char buf[9] = {'s', 'o', 'u', 'n', 'd', '1', '1', '3', ':'}; 
	xQueueReset(__uartQueue);

	__xfsSendByte(0xFD);
  if(type == 3){
		ret = len + 38;
	} else {
		ret = len + 20;
	}
	__xfsSendByte(ret >> 8);
	__xfsSendByte(ret & 0xFF);
	__xfsSendByte(0x01);
	__xfsSendByte(type);
	
	for(ret = 0; ret < 9; ret++){
		vTaskDelay(configTICK_RATE_HZ / 100);
		__xfsSendByte(buf[ret]);
		if(type == 3){
			vTaskDelay(configTICK_RATE_HZ / 100);
		  __xfsSendByte(0);
		}
	}
	
	buf[6] = '0';
	buf[7] = '5';
	for(ret = 0; ret < 9; ret++){
		vTaskDelay(configTICK_RATE_HZ / 100);
		__xfsSendByte(buf[ret]);
		if(type == 3){
			vTaskDelay(configTICK_RATE_HZ / 100);
		  __xfsSendByte(0);
		}
	}

	for (ret = 0; ret < len; ret++) {
		vTaskDelay(configTICK_RATE_HZ / 100);
		__xfsSendByte(*p++);
	}	
	ret = 0;
	
	rc = xQueueReceive(__uartQueue, &ret, configTICK_RATE_HZ * 10);
	
	vTaskDelay(configTICK_RATE_HZ * 2);
	if (rc != pdTRUE) {
		return 0;
	}
	if (ret != 0x41) {
		return 0;
	}
	ret = 0;
// 	while (ret <= len) {
// 		if (__xfsQueryState() != 0) {
// 			return 1;
// 		}
// 		vTaskDelay(configTICK_RATE_HZ / 2);
// 		++ret;
// 	}
	
	while (ret <= len) {
		if (__xfsQueryState() == 0x4F) {
			return 1;
		}
		vTaskDelay(configTICK_RATE_HZ / 2);
		++ret;
	}
	return 1;
}
#endif

static char off = 0;

char *playOff(){
	return &off;
}
static int __xfsSpeakLowLevelWithTimes(const char *p, int len, char type) {
	int i;
	for (i = 0; i < speakParam.speakTimes; ++i) {
		if (!__xfsSpeakLowLevel(p, len, type)) {
			return 0;
		}
		vTaskDelay(configTICK_RATE_HZ * speakParam.speakPause);
	}
	off = 1;
	return 1;
}

static void __handleSpeakMessage(XfsTaskMessage *msg) {
	switch (msg->type) {
	case TYPE_MSG_GB2312:
		__xfsSpeakLowLevelWithTimes(__messageGetSpeakerData(msg), msg->len, TYPE_GB2312);
		break;
	case TYPE_MSG_GBK:
		__xfsSpeakLowLevelWithTimes(__messageGetSpeakerData(msg), msg->len, TYPE_GBK);
		break;
	case TYPE_MSG_BIG5:
		__xfsSpeakLowLevelWithTimes(__messageGetSpeakerData(msg), msg->len, TYPE_BIG5);
		break;
	case TYPE_MSG_UCS2:
		__xfsSpeakLowLevelWithTimes(__messageGetSpeakerData(msg), msg->len, TYPE_UCS2);
		break;
	case TYPE_SET_SPEAKTIMES:
		__setSpeakTimesLowLevel(__messageGetSpeakerTimes(msg));
		break;
	case TYPE_SET_SPEAKPAUSE:
		__setSpeakPauseLowLevel(__messageGetSpeakerPause(msg));
		break;
	case TYPE_SET_SPEAKVOLUME:
		__setSpeakVolumeLowLevel(__messageGetSpeakerVolume(msg));
		break;
	case TYPE_SET_SPEAKTONE:
		__setSpeakToneLowLevel(__messageGetSpeakerTone(msg));
		break;
	case TYPE_SET_SPEAKTYPE:
		__setSpeakTypeLowLevel(__messageGetSpeakerType(msg));
		break;
	case TYPE_SET_SPEAKSPEED:
		__setSpeakSpeedLowLevel(__messageGetSpeakerType(msg));
		break;
	default:
		break;
	}
}

void __xfsTask(void *parameter) {
	portBASE_TYPE rc;
	XfsTaskMessage *pmsg;
	__uartQueue = xQueueCreate(5, sizeof(char));
	__speakQueue = xQueueCreate(3, sizeof(char *));

	printf("Xfs start\n");
	__xfsInitRuntime();
	__restorSpeakParam();
	for (;;) {
		printf("Xfs: loop again\n");
		rc = xQueueReceive(__speakQueue, &pmsg, portMAX_DELAY);
		if (rc == pdTRUE) {
			SoundControlSetChannel(SOUND_CONTROL_CHANNEL_XFS, 1);
			GPIO_SetBits(GPIOG, SMS_PIN);
			__handleSpeakMessage(pmsg);
			SoundControlSetChannel(SOUND_CONTROL_CHANNEL_XFS, 0);
			GPIO_ResetBits(GPIOG, SMS_PIN);
			vPortFree(pmsg);
		}
	}
}

void XfsTaskSetSpeakTimes(int times) {
	XfsTaskMessage *p = (XfsTaskMessage *)pvPortMalloc(sizeof(XfsTaskMessage));
	p->type = TYPE_SET_SPEAKTIMES;
	p->len = times;
	if (pdTRUE != xQueueSend(__speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakPause(int sec) {
	XfsTaskMessage *p = (XfsTaskMessage *)pvPortMalloc(sizeof(XfsTaskMessage));
	p->type = TYPE_SET_SPEAKPAUSE;
	p->len = sec;
	if (pdTRUE != xQueueSend(__speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakVolume(char vol) {
	XfsTaskMessage *p = (XfsTaskMessage *)pvPortMalloc(sizeof(XfsTaskMessage));
	p->type = TYPE_SET_SPEAKVOLUME;
	p->len = vol;
	if (pdTRUE != xQueueSend(__speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakType(char type) {
	XfsTaskMessage *p = (XfsTaskMessage *)pvPortMalloc(sizeof(XfsTaskMessage));
	p->type = TYPE_SET_SPEAKTYPE;
	p->len = type;
	if (pdTRUE != xQueueSend(__speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakTone(char tone) {
	XfsTaskMessage *p = (XfsTaskMessage *)pvPortMalloc(sizeof(XfsTaskMessage));
	p->type = TYPE_SET_SPEAKTONE;
	p->len = tone;
	if (pdTRUE != xQueueSend(__speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSetSpeakSpeed(char speed) {
	XfsTaskMessage *p = (XfsTaskMessage *)pvPortMalloc(sizeof(XfsTaskMessage));
	p->type = TYPE_SET_SPEAKTONE;
	p->len = speed;
	if (pdTRUE != xQueueSend(__speakQueue, &p, configTICK_RATE_HZ * 5)) {
		vPortFree(p);
	}
}

void XfsTaskSpeakUCS2(const char *s, int len) {
	__xfsSpeak(s, len, TYPE_MSG_UCS2);
}

void XfsTaskSpeakGBK(const char *s, int len) {
	__xfsSpeak(s, len, TYPE_MSG_GBK);
}

unsigned char *XFSpara(unsigned char *p){
	unsigned char len;
	__restorSpeakParam();
	p = pvPortMalloc(36);
	len = sprintf(p, "{%d,", speakParam.speakTimes);
	len += sprintf(&p[len], "%d,", speakParam.speakPause);
	len += sprintf(&p[len], "%c,", speakParam.speakVolume);
	len += sprintf(&p[len], "%c,", speakParam.speakType);
	len += sprintf(&p[len], "%c,", speakParam.speakSpeed);
	len += sprintf(&p[len], "%c}", speakParam.speakTone);
	return p;
}

void XfsInit(void) {
	__initHardware();
	xTaskCreate(__xfsTask, (signed portCHAR *) "XFS", XFS_TASK_STACK_SIZE, (void *)'2', tskIDLE_PRIORITY + 2, NULL);
}

void USART3_IRQHandler(void) {
	char data;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if (USART_GetITStatus(USART3, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(USART3);
	USART_ClearITPendingBit(USART3, USART_IT_RXNE);
	if (pdTRUE == xQueueSendFromISR(__uartQueue, &data, &xHigherPriorityTaskWoken)) {
		if (xHigherPriorityTaskWoken) {
			taskYIELD();
		}
	}
}


