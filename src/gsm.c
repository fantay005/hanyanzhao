#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "protocol.h"
#include "misc.h"
#include "sms.h"
#include "xfs.h"
#include "zklib.h"
#include "atcmd.h"
#include "norflash.h"

#define GSM_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE + 256 )

#if defined(__SPEAKER__)
#  define RESET_GPIO_GROUP GPIOA
#  define RESET_GPIO GPIO_Pin_11
#elif defined(__LED__)
#  define RESET_GPIO_GROUP GPIOB
#  define RESET_GPIO GPIO_Pin_1
#endif

#define GSM_SET_RESET()  GPIO_SetBits(RESET_GPIO_GROUP, RESET_GPIO)
#define GSM_CLEAR_RESET()  GPIO_ResetBits(RESET_GPIO_GROUP, RESET_GPIO)

#define GSM_POWER_ON() GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define GSM_POWER_OFF() GPIO_ResetBits(GPIOB, GPIO_Pin_0)

#define HEART_BEAT_TIME  (configTICK_RATE_HZ * 60 * 9 / 10)

void ATCmdSendChar(char c) {
	USART_SendData(USART2, c);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

static xQueueHandle __gsmTaskQueue;

#define IMEI_LENGTH 15
static char __imei[IMEI_LENGTH + 1];

#define GsmPortMalloc(size) pvPortMalloc(size)
#define __GsmPortFree(p) vPortFree(p)

static struct {
	unsigned char IMEI[16];
	unsigned char serverIP[16];
	unsigned int serverPORT;
} GSMParam = {"0", "221.130.129.72", 5555};

static inline void storeGSMParam(void) {
	NorFlashWrite(GSM_PARAM_STORE_ADDR, (const short *)&GSMParam, sizeof(GSMParam));
}

void restorGSMParam(void) {
	NorFlashRead(GSM_PARAM_STORE_ADDR, (short *)&GSMParam, sizeof(GSMParam));
}

void setGSMserverIPLowLevel(char *ip, int port) {
	strcpy(GSMParam.serverIP, ip);
	GSMParam.serverPORT = port;
	storeGSMParam();
}

typedef enum {
	TYPE_NONE = 0,
	TYPE_SMS_DATA,
	TYPE_RING,
	TYPE_GPRS_DATA,
	TYPE_SEND_TCP_DATA,
	TYPE_RESET,
	TYPE_NO_CARRIER,
} GsmTaskMessageType;

typedef struct {
	GsmTaskMessageType type;
	unsigned int length;
} GsmTaskMessage;


static inline char *messageGetData(GsmTaskMessage *message) {
	return (char *)(&message[1]);
}

GsmTaskMessage *GsmCreateMessage(GsmTaskMessageType type, const char *dat, int len) {
	GsmTaskMessage *message = GsmPortMalloc(ALIGNED_SIZEOF(GsmTaskMessage) + len);
	if (message != NULL) {
		message->type = type;
		message->length = len;
		memcpy(&message[1], dat, len);
	}
	return message;
}

void GmsDestroyMessage(GsmTaskMessage *message) {
	__GsmPortFree(message);
}

int GsmTaskResetSystemAfter(int seconds) {
	GsmTaskMessage *message = GsmCreateMessage(TYPE_RESET, 0, 0);
	message->length = seconds;
	if (pdTRUE != xQueueSend(__gsmTaskQueue, &message, configTICK_RATE_HZ * 5)) {
		GmsDestroyMessage(message);
		return 0;
	}
	return 1;
}

int GsmTaskSendTcpData(const char *p, int len) {
	GsmTaskMessage *message = GsmCreateMessage(TYPE_SEND_TCP_DATA, p, len);
	if (pdTRUE != xQueueSend(__gsmTaskQueue, &message, configTICK_RATE_HZ * 5)) {
		GmsDestroyMessage(message);
		return 0;
	}
	return 1;
}

static void initHardware() {
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	__gsmTaskQueue = xQueueCreate(5, sizeof(GsmTaskMessage *));		  //队列创建

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);				   //GSM模块的串口

	USART_InitStructure.USART_BaudRate = 19200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART2, ENABLE);

	GSM_CLEAR_RESET();
	GPIO_InitStructure.GPIO_Pin =  RESET_GPIO;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(RESET_GPIO_GROUP, &GPIO_InitStructure);				    //GSM模块的RTS和RESET

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOG, &GPIO_InitStructure);				    //GSM模块的RTS和RESET

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);				   //GSM模块的STATAS

	GPIO_ResetBits(GPIOB, GPIO_Pin_0);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);				   //GSM_power_on,29302

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

typedef struct {
	const char *prefix;
	GsmTaskMessageType type;
} AutoReportMap;
static const AutoReportMap __autoReportMaps[] = {
	{ "+CMT", TYPE_SMS_DATA },
	{ "RING", TYPE_RING },
	{ "NO CARRIER", TYPE_NO_CARRIER },
	{ NULL, TYPE_NONE },
};

void USART2_IRQHandler(void) {
	static char buffer[1300];
	static int bufferIndex = 0;

	static char isIPD = 0;
	static int lenIPD;

	unsigned char data;

	if (USART_GetITStatus(USART2, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(USART2);
	USART_SendData(USART1, data);
	USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	if (isIPD) {
		if (isIPD == 1) {
			lenIPD = data << 8;
			isIPD = 2;
		} else if (isIPD == 2) {
			lenIPD += data;
			isIPD = 3;
		}
		buffer[bufferIndex++] = data;
		if ((isIPD == 3) && (bufferIndex >= lenIPD + 14)) {
			portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
			GsmTaskMessage *message;
			buffer[bufferIndex++] = 0;
			message = GsmCreateMessage(TYPE_GPRS_DATA, buffer, bufferIndex);
			if (pdTRUE == xQueueSendFromISR(__gsmTaskQueue, &message, &xHigherPriorityTaskWoken)) {
				if (xHigherPriorityTaskWoken) {
					taskYIELD();
				}
			}
			isIPD = 0;
			bufferIndex = 0;
		}
		return;
	}
	if (data == '\n') {
		buffer[bufferIndex++] = 0;
		if (bufferIndex >= 2) {
			portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
			const AutoReportMap *p;
			for (p = __autoReportMaps; p->prefix != NULL; ++p) {
				if (strncmp(p->prefix, buffer, strlen(p->prefix)) == 0) {
					GsmTaskMessage *message = GsmCreateMessage(p->type, buffer, bufferIndex);
					xQueueSendFromISR(__gsmTaskQueue, &message, &xHigherPriorityTaskWoken);
					break;
				}
			}
			if (p->prefix == NULL) {
				ATCommandGotLineFromIsr(buffer, bufferIndex, &xHigherPriorityTaskWoken);
			}

			if (xHigherPriorityTaskWoken) {
				taskYIELD();
			}
		}
		bufferIndex = 0;
	} else if (data != '\r') {
		buffer[bufferIndex++] = data;
		if ((bufferIndex == 2) && (strncmp("#H", buffer, 2) == 0)) {
			isIPD = 1;
		}
	}
}

void UartQueueReset(xQueueHandle queue) {
	GsmTaskMessage *message;
	while (pdTRUE == xQueueReceive(queue, &message, 1)) {
		GmsDestroyMessage(message);
	}
}

void startGsm() {
	GSM_POWER_OFF();
	vTaskDelay(configTICK_RATE_HZ / 2);

	GSM_POWER_ON();
	vTaskDelay(configTICK_RATE_HZ / 2);

	GSM_SET_RESET();
	vTaskDelay(configTICK_RATE_HZ * 2);

	GSM_CLEAR_RESET();
	vTaskDelay(configTICK_RATE_HZ * 5);
}
int isTcpConnected() {
	char *reply;
	while (1) {
		reply = ATCommand("AT+QISTAT\r", "STATE:", configTICK_RATE_HZ * 2);
		if (reply == NULL) {
			return 0;
		}
		if (strncmp(&reply[7], "CONNECT OK", 10) == 0) {
			AtCommandDropReplyLine(reply);
			return 1;
		}
		if (strncmp(&reply[7], "TCP CONNECTING", 12) == 0) {
			AtCommandDropReplyLine(reply);
			vTaskDelay(configTICK_RATE_HZ);
			continue;
		}
		AtCommandDropReplyLine(reply);
		break;
	}
	return 0;
}

int sendTcpData(const char *p, int len) {
	int i;
	char buf[16];
	char *reply;

	sprintf(buf, "AT+QISEND=%d\r", len);
	ATCommand(buf, NULL, configTICK_RATE_HZ / 5);
	for (i = 0; i < len; i++) {
		ATCmdSendChar(*p++);
	}

	while (1) {
		reply = ATCommand(NULL, "SEND", configTICK_RATE_HZ * 3);
		if (reply == NULL) {
			return 0;
		}
		if (0 == strncmp(reply, "SEND OK", 7)) {
			AtCommandDropReplyLine(reply);
			return 1;
		} else if (0 == strncmp(reply, "SEND FAIL", 9)) {
			AtCommandDropReplyLine(reply);
			return 0;
		} else if (0 == strncmp(reply, "ERROR", 5)) {
			AtCommandDropReplyLine(reply);
			return 0;
		} else {
			AtCommandDropReplyLine(reply);
		}
	}
}

int checkTcpAndConnect(const char *ip, unsigned short port) {
	char buf[44];
	char *reply;
	if (isTcpConnected()) {
		return 1;
	}
	ATCommand("AT+QIDEACT\r", NULL, configTICK_RATE_HZ * 2);
	sprintf(buf, "AT+QIOPEN=\"TCP\",\"%s\",\"%d\"\r", ip, port);
	reply = ATCommand(buf, "CONNECT", configTICK_RATE_HZ * 40);
	if (reply == NULL) {
		return 0;
	}
	if (strncmp("CONNECT OK", reply, 10) == 0) {
		int size;
		const char *data;
		AtCommandDropReplyLine(reply);
		data = ProtoclCreatLogin(__imei, &size);
		sendTcpData(data, size);
		ProtocolDestroyMessage(data);

		return 1;
	}
	AtCommandDropReplyLine(reply);
	return 0;
}

int __initGsmRuntime() {

	ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ / 2);
	ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ / 2);

	if (!ATCommandAndCheckReply("ATE0\r", "OK", configTICK_RATE_HZ)) {
		printf("ATE0  error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply(NULL, "Call Ready", configTICK_RATE_HZ * 20)) {
		printf("Wait Call Realy timeout\n");
	}

	if (!ATCommandAndCheckReply("ATS0=3\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("ATS0=3 error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+CMGF=0\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+CMGF=0 error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+CNMI=2,2,0,0,0\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+CNMI error\r");
		return 0;
	}

	if (!ATCommandAndCheckReplyUntilOK("AT+CPMS=\"SM\"\r", "+CPMS", configTICK_RATE_HZ * 3, 10)) {
		printf("AT+CPMS error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+CSQ\r", "+CSQ:", configTICK_RATE_HZ / 5)) {
		printf("AT+CSQ error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+QIDEACT\r", "DEACT", configTICK_RATE_HZ / 5)) {
		printf("AT+QIDEACT error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+QIHEAD=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIHEAD error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+QISHOWRA=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWRA error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+QISHOWPT=1\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWPT error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+QIFGCNT=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIFGCNT error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+QICSGP=1,\"CMNET\"\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QICSGP error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT+QIMUX=0\r", "OK", configTICK_RATE_HZ)) {
		printf("AT+QIMUX error\r");
		return 0;
	}

	if (!ATCommandAndCheckReply("AT&W\r", "OK", configTICK_RATE_HZ)) {
		printf("AT&W error\r");
		return 0;
	}
	return 1;
}
// 删除短信？、？？;
void __handleSMS(GsmTaskMessage *p) {
	char *reply;
	sms_t *sms;

	reply = ATCommand(NULL, ATCMD_ANY_REPLY_PREFIX, configTICK_RATE_HZ);
	if (reply != NULL) {
		sms = GsmPortMalloc(sizeof(sms_t));
		printf("Gsm: got sms => %s\n", reply);
		SMSDecodePdu(reply, sms);
		printf("Gsm: sms_content=> %s\n", sms->sms_content);
#if defined(__SPEAKER__)
		XfsTaskSpeakUCS2(sms->sms_content, sms->content_len);
#elif defined(__LED__)
		ProtocolHandlerSMS(sms);
#endif
		__GsmPortFree(sms);
	}
	AtCommandDropReplyLine(reply);
}

int isValidIMEI(const char *p) {
	int i;
	if (strlen(p) != IMEI_LENGTH) {
		return 0;
	}

	for (i = 0; i < IMEI_LENGTH; i++) {
		if (!isdigit(p[i])) {
			return 0;
		}
	}

	return 1;
}

int gsmGetIMEI() {
	char *reply;
	int i;
	reply = ATCommand("AT+GSN\r", ATCMD_ANY_REPLY_PREFIX, configTICK_RATE_HZ / 10);
	if (reply == NULL) {
		return 0;
	}
	if (!isValidIMEI(reply)) {
		return 0;
	}
	strcpy(__imei, reply);
	for (i = 0; i < 16; i++) {
		GSMParam.IMEI[i] = __imei[i];
	}
	storeGSMParam();
	AtCommandDropReplyLine(reply);
	return 1;
}

void __handleProtocol(GsmTaskMessage *msg) {
	ProtocolHandler(messageGetData(msg));
}

void __handleSendTcpData(GsmTaskMessage *msg) {
	sendTcpData(messageGetData(msg), msg->length);
}

void __handleReset(GsmTaskMessage *msg) {
	unsigned int len = msg->length;
	if (len > 100) {
		len = 100;
	}
	vTaskDelay(configTICK_RATE_HZ * len);
	while (1) {
		NVIC_SystemReset();
		vTaskDelay(configTICK_RATE_HZ);
	}
}

void __handleResetNoCarrier(GsmTaskMessage *msg) {
	GPIO_SetBits(GPIOD, GPIO_Pin_2);
}

void __handleRING(GsmTaskMessage *msg) {
	GPIO_ResetBits(GPIOD, GPIO_Pin_2);
}

typedef struct {
	GsmTaskMessageType type;
	void (*handlerFunc)(GsmTaskMessage *);
} MessageHandlerMap;

static const MessageHandlerMap __messageHandlerMaps[] = {
	{ TYPE_SMS_DATA, __handleSMS },
	{ TYPE_RING, __handleRING },
	{ TYPE_GPRS_DATA, __handleProtocol },
	{ TYPE_SEND_TCP_DATA, __handleSendTcpData },
	{ TYPE_RESET, __handleReset },
	{ TYPE_NO_CARRIER, __handleResetNoCarrier },
	{ TYPE_NONE, NULL },
};

static void __gsmTask(void *parameter) {
	portBASE_TYPE rc;
	GsmTaskMessage *message;
	portTickType lastT = 0;

	while (1) {
		printf("Gsm start\n");
		startGsm();
		if (__initGsmRuntime()) {
			break;
		}
	}

	while (!gsmGetIMEI()) {
		vTaskDelay(configTICK_RATE_HZ);
	}

	for (;;) {
		printf("Gsm: loop again\n");
		rc = xQueueReceive(__gsmTaskQueue, &message, configTICK_RATE_HZ * 10);
		if (rc == pdTRUE) {
			const MessageHandlerMap *map = __messageHandlerMaps;
			for (; map->type != TYPE_NONE; ++map) {
				if (message->type == map->type) {
					map->handlerFunc(message);
					break;
				}
			}
			GmsDestroyMessage(message);
		} else {
			int curT = xTaskGetTickCount();
			if (0 == checkTcpAndConnect(GSMParam.serverIP, GSMParam.serverPORT)) {
				printf("Gsm: Connect TCP error\n");
			} else if ((curT - lastT) >= HEART_BEAT_TIME) {
				int size;
				const char *dat = ProtoclCreateHeartBeat(&size);
				sendTcpData(dat, size);
				ProtocolDestroyMessage(dat);
				lastT = curT;
			}
		}
	}
}

void GSMInit(void) {
	ATCommandRuntimeInit();
	initHardware();
	xTaskCreate(__gsmTask, (signed portCHAR *) "GSM", GSM_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
}
