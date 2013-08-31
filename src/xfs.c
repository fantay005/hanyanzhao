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

typedef struct {
	short type;
	short len;
} SpeakMessage;

void xfsSpeak(const char *s, int len, XfsEncodeType type) {
	SpeakMessage *p = (SpeakMessage *)pvPortMalloc(sizeof(SpeakMessage) + len);
	p->type = type;
	p->len = len;
	memcpy(&p[1], s, len);
	xQueueSend(speakQueue, &p, configTICK_RATE_HZ*5);
}

void USART3_IRQHandler(void) {
	char data;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if(USART_GetITStatus(USART3, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(USART3);
	USART_SendData(USART1, data);
	USART_ClearITPendingBit(USART3,USART_IT_RXNE);
	if(pdTRUE == xQueueSendFromISR(uartQueue, &data, &xHigherPriorityTaskWoken)) {
		if(xHigherPriorityTaskWoken ) {
			taskYIELD();
		}
	}
}


static int xfsSendCommand(const char *dat, int size, int timeoutTick) {
	char ret;
	portBASE_TYPE rc;
	xQueueReset(uartQueue);
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, 0xFD);
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, size >> 8);
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, size & 0xFF);

	while (size-- > 0) {
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
		USART_SendData(USART3, *dat++);
	}

	rc = xQueueReceive(uartQueue, &ret, timeoutTick);
	if (rc != pdTRUE) return -1;
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
	                             '[', 's', '5', ']', '[', 'm', '3', ']',
	                          };
	char ret = xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	printf("xfsSetup return %02X\n", ret);
	return 0;
}

static int xfsQueryState() {
	const char xfsCommand[] = { 0x21 };
	char ret = xfsSendCommand(xfsCommand, sizeof(xfsCommand), configTICK_RATE_HZ);
	printf("xfsQueryState return %02X\n", ret);
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
	GPIO_Init(GPIOC, &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);					//讯飞语音模块的串口

	USART_InitStructure.USART_BaudRate =9600;
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
	GPIO_Init(GPIOC, &GPIO_InitStructure );					 //讯飞语音模块的RESET

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure );					 //讯飞语音模块的RDY

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
	GPIO_ResetBits(GPIOC, GPIO_Pin_7);
	vTaskDelay(configTICK_RATE_HZ/5);
	GPIO_SetBits(GPIOC, GPIO_Pin_7);

	xfsWoken();
	vTaskDelay(configTICK_RATE_HZ);
	xfsSetup();
	vTaskDelay(configTICK_RATE_HZ);
}

static int xfsSpeakLowLevel(SpeakMessage *msg) {
	int ret;
	portBASE_TYPE rc;
	const char *p;
	xQueueReset(uartQueue);
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, 0xFD);
	ret = msg->len + 2;
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, ret >> 8);
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, ret & 0xFF);
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, 0x01);
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, msg->type);

	p = (const char *)&msg[1];
	for(ret = 0; ret < msg->len; ret++) {
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
		USART_SendData(USART3, *p++);
	}

	rc = xQueueReceive(uartQueue, &ret, configTICK_RATE_HZ);
	if (rc != pdTRUE) return 0;
	if (ret != 0x41) return 0;

	ret = 0;
	while (ret < msg->len/2) {
		if (xfsQueryState() == 0x4F) return 1;
		vTaskDelay(configTICK_RATE_HZ);
		++ret;
	}
	return 0;
}

void vXfs(void *parameter) {
	portBASE_TYPE rc;
	SpeakMessage *pmsg;
	uartQueue = xQueueCreate(5, sizeof(char));		  //队列创建
	speakQueue = xQueueCreate(3, sizeof(char *));		  //队列创建

	printf("Xfs start\n");
	initHardware();
	xfsInitRuntime();
	for ( ;; ) {
		printf("Xfs: loop again\n");
		rc = xQueueReceive(speakQueue, &pmsg, configTICK_RATE_HZ*5);
		if (rc == pdTRUE) {
			//printf("Xfs: get reply %02X\n", dat);
			xfsSpeakLowLevel(pmsg);
			vPortFree(pmsg);
		} else {
			//xfsSpeakGBK();
		}
	}
}
