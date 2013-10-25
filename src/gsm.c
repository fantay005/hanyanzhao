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
#include "unicode2gbk.h"

#define GSM_TASK_STACK_SIZE			 (configMINIMAL_STACK_SIZE + 256)
#define GSM_GPRS_HEART_BEAT_TIME     (configTICK_RATE_HZ * 60 * 9 / 10)
#define GSM_IMEI_LENGTH              15

#if defined(__SPEAKER__)
#  define RESET_GPIO_GROUP           GPIOA
#  define RESET_GPIO                 GPIO_Pin_11
#elif defined(__LED__)
#  define RESET_GPIO_GROUP           GPIOB
#  define RESET_GPIO                 GPIO_Pin_1
#endif

#define __gsmAssertResetPin()        GPIO_SetBits(RESET_GPIO_GROUP, RESET_GPIO)
#define __gsmDeassertResetPin()      GPIO_ResetBits(RESET_GPIO_GROUP, RESET_GPIO)
#define __gsmPowerSupplyOn()         GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define __gsmPowerSupplyOff()        GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define __gsmPortMalloc(size)        pvPortMalloc(size)
#define __gsmPortFree(p)             vPortFree(p)



void __gsmSMSEncodeConvertToGBK(SMSInfo *info) {
	uint8_t *gbk;

	if (info->encodeType == ENCODE_TYPE_GBK) {
		return;
	}

	gbk = Unicode2GBK(info->content, info->contentLen);
	strcpy(info->content, gbk);
	Unicode2GBKDestroy(gbk);
	info->encodeType = ENCODE_TYPE_GBK;
	info->contentLen = strlen(info->content);
}


/// GSM task message queue.
static xQueueHandle __queue;

/// Save the imei of GSM modem, filled when GSM modem start.
static char __imei[GSM_IMEI_LENGTH + 1];

/// Save runtime parameters for GSM task;
static GMSParameter __gsmRuntimeParameter = {"221.130.129.72", 5555};

/// Basic function for sending AT Command, need by atcmd.c.
/// \param  c    Char data to send to modem.
void ATCmdSendChar(char c) {
	USART_SendData(USART2, c);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

/// Store __gsmRuntimeParameter to flash.
static inline void __storeGsmRuntimeParameter(void) {
	NorFlashWrite(GSM_PARAM_STORE_ADDR, (const short *)&__gsmRuntimeParameter, sizeof(__gsmRuntimeParameter));
}

/// Restore __gsmRuntimeParameter from flash.
static inline void __restorGsmRuntimeParameter(void) {
	NorFlashRead(GSM_PARAM_STORE_ADDR, (short *)&__gsmRuntimeParameter, sizeof(__gsmRuntimeParameter));
}

/// Low level set TCP server IP and port.
static void __setGSMserverIPLowLevel(char *ip, int port) {
	strcpy(__gsmRuntimeParameter.serverIP, ip);
	__gsmRuntimeParameter.serverPORT = port;
	__storeGsmRuntimeParameter();
}

typedef enum {
	TYPE_NONE = 0,
	TYPE_SMS_DATA,
	TYPE_RING,
	TYPE_GPRS_DATA,
	TYPE_SEND_TCP_DATA,
	TYPE_RESET,
	TYPE_NO_CARRIER,
	TYPE_SEND_AT,
	TYPE_SEND_SMS,
} GsmTaskMessageType;

/// Message format send to GSM task.
typedef struct {
	/// Message type.
	GsmTaskMessageType type;
	/// Message lenght.
	unsigned int length;
} GsmTaskMessage;


/// Get the data of a message.
/// \param  message    Which message to get data from.
/// \return The associate data of the message.
static inline void *__gsmGetMessageData(GsmTaskMessage *message) {
	return &message[1];
}

/// Create a message.
/// \param  type   The type of message to create.
/// \param  data   Associate this data to the message.
/// \param  len    Then lenght(byte number) of the data.
/// \return !=NULL The message which created.
/// \return ==NULL Create message failed.
GsmTaskMessage *__gsmCreateMessage(GsmTaskMessageType type, const char *dat, int len) {
	GsmTaskMessage *message = __gsmPortMalloc(ALIGNED_SIZEOF(GsmTaskMessage) + len);
	if (message != NULL) {
		message->type = type;
		message->length = len;
		memcpy(&message[1], dat, len);
	}
	return message;
}

/// Destroy a message.
/// \param  message   The message to destory.
void __gsmDestroyMessage(GsmTaskMessage *message) {
	__gsmPortFree(message);
}

int GsmTaskResetSystemAfter(int seconds) {
	GsmTaskMessage *message = __gsmCreateMessage(TYPE_RESET, 0, 0);
	message->length = seconds;
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 5)) {
		__gsmDestroyMessage(message);
		return 0;
	}
	return 1;
}


/// Send a AT command to GSM modem.
/// \param  atcmd  AT command to send.
/// \return true   When operation append to GSM task message queue.
/// \return false  When append operation to GSM task message queue failed.
bool GsmTaskSendAtCommand(const char *atcmd) {
	int len = strlen(atcmd);
	GsmTaskMessage *message = __gsmCreateMessage(TYPE_SEND_AT, atcmd, len + 2);
	char *dat = __gsmGetMessageData(message);
	dat[len] = '\r';
	dat[len + 1] = 0;
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 5)) {
		__gsmDestroyMessage(message);
		return false;
	}
	return true;

}

/// Send a AT command to GSM modem.
/// \param  atcmd  AT command to send.
/// \return true   When operation append to GSM task message queue.
/// \return false  When append operation to GSM task message queue failed.
bool GsmTaskSendSMS(const char *pdu, int len) {
	GsmTaskMessage *message = __gsmCreateMessage(TYPE_SEND_SMS, pdu, len);
	char *dat = __gsmGetMessageData(message);
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 15)) {
		__gsmDestroyMessage(message);
		return false;
	}
	return true;
}

/// Send data to TCP server.
/// \param  dat    Data to send.
/// \param  len    Then length of the data.
/// \return true   When operation append to GSM task message queue.
/// \return false  When append operation to GSM task message queue failed.
bool GsmTaskSendTcpData(const char *dat, int len) {
	GsmTaskMessage *message = __gsmCreateMessage(TYPE_SEND_TCP_DATA, dat, len);
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 5)) {
		__gsmDestroyMessage(message);
		return true;
	}
	return false;
}

/// Init the CPU on chip hardware for the GSM modem.
static void __gsmInitHardware(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

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

	__gsmDeassertResetPin();
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
	GPIO_Init(GPIOB, &GPIO_InitStructure);				   //__gsmPowerSupplyOn,29302

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
} GSMAutoReportMap;

static const GSMAutoReportMap __gsmAutoReportMaps[] = {
//	{ "+CMT", TYPE_SMS_DATA },
	{ "RING", TYPE_RING },
	{ "NO CARRIER", TYPE_NO_CARRIER },
	{ NULL, TYPE_NONE },
};



static char buffer[1300];
static int bufferIndex = 0;
static char isIPD = 0;
static char isSMS = 0;
static int lenIPD;

static inline void __gmsReceiveIPDData(unsigned char data) {
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
		message = __gsmCreateMessage(TYPE_GPRS_DATA, buffer, bufferIndex);
		if (pdTRUE == xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				taskYIELD();
			}
		}
		isIPD = 0;
		bufferIndex = 0;
	}
}


static inline void __gmsReceiveSMSData(unsigned char data) {
	if (data == 0x0A) {
		GsmTaskMessage *message;
		portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
		buffer[bufferIndex++] = 0;
		message = __gsmCreateMessage(TYPE_SMS_DATA, buffer, bufferIndex);
		xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken);
		isSMS = 0;
		bufferIndex = 0;
	} else if (data != 0x0D) {
		buffer[bufferIndex++] = data;
	}
}

void USART2_IRQHandler(void) {
	unsigned char data;
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(USART2);
	USART_SendData(USART1, data);
	USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	if (isIPD) {
		__gmsReceiveIPDData(data);
		return;
	}

	if (isSMS) {
		__gmsReceiveSMSData(data);
		return;
	}

	if (data == 0x0A) {
		buffer[bufferIndex++] = 0;
		if (bufferIndex >= 2) {
			portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
			const GSMAutoReportMap *p;
			if (strncmp(buffer, "+CMT:", 5) == 0) {
				bufferIndex = 0;
				isSMS = 1;
			}
			for (p = __gsmAutoReportMaps; p->prefix != NULL; ++p) {
				if (strncmp(p->prefix, buffer, strlen(p->prefix)) == 0) {
					GsmTaskMessage *message = __gsmCreateMessage(p->type, buffer, bufferIndex);
					xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken);
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
	} else if (data != 0x0D) {
		buffer[bufferIndex++] = data;
		if ((bufferIndex == 2) && (strncmp("#H", buffer, 2) == 0)) {
			isIPD = 1;
		}
	}
}

/// Start GSM modem.
void __gsmModemStart() {
	__gsmPowerSupplyOff();
	vTaskDelay(configTICK_RATE_HZ / 2);

	__gsmPowerSupplyOn();
	vTaskDelay(configTICK_RATE_HZ / 2);

	__gsmAssertResetPin();
	vTaskDelay(configTICK_RATE_HZ * 2);

	__gsmDeassertResetPin();
	vTaskDelay(configTICK_RATE_HZ * 5);
}

/// Check if has the GSM modem connect to a TCP server.
/// \return true   When the GSM modem has connect to a TCP server.
/// \return false  When the GSM modem dose not connect to a TCP server.
bool __gsmIsTcpConnected() {
	char *reply;
	while (1) {
		reply = ATCommand("AT+QISTAT\r", "STATE:", configTICK_RATE_HZ * 2);
		if (reply == NULL) {
			return false;
		}
		if (strncmp(&reply[7], "CONNECT OK", 10) == 0) {
			AtCommandDropReplyLine(reply);
			return true;
		}
		if (strncmp(&reply[7], "TCP CONNECTING", 12) == 0) {
			AtCommandDropReplyLine(reply);
			vTaskDelay(configTICK_RATE_HZ);
			continue;
		}
		AtCommandDropReplyLine(reply);
		break;
	}
	return false;
}

bool __gsmSendTcpDataLowLevel(const char *p, int len) {
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
			return false;
		}
		if (0 == strncmp(reply, "SEND OK", 7)) {
			AtCommandDropReplyLine(reply);
			return true;
		} else if (0 == strncmp(reply, "SEND FAIL", 9)) {
			AtCommandDropReplyLine(reply);
			return false;
		} else if (0 == strncmp(reply, "ERROR", 5)) {
			AtCommandDropReplyLine(reply);
			return false;
		} else {
			AtCommandDropReplyLine(reply);
		}
	}
}

bool __gsmCheckTcpAndConnect(const char *ip, unsigned short port) {
	char buf[44];
	char *reply;
	if (__gsmIsTcpConnected()) {
		return true;
	}
	ATCommand("AT+QIDEACT\r", NULL, configTICK_RATE_HZ * 2);
	sprintf(buf, "AT+QIOPEN=\"TCP\",\"%s\",\"%d\"\r", ip, port);
	reply = ATCommand(buf, "CONNECT", configTICK_RATE_HZ * 40);
	if (reply == NULL) {
		return false;
	}

	if (strncmp("CONNECT OK", reply, 10) == 0) {
		int size;
		const char *data;
		AtCommandDropReplyLine(reply);
		data = ProtoclCreatLogin(__imei, &size);
		__gsmSendTcpDataLowLevel(data, size);
		ProtocolDestroyMessage(data);
		return true;
	}
	AtCommandDropReplyLine(reply);
	return false;
}

bool __initGsmRuntime() {
	ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ / 2);
	ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ / 2);

	if (!ATCommandAndCheckReply("ATE0\r", "OK", configTICK_RATE_HZ)) {
		printf("ATE0  error\r");
		return false;
	}

	if (!ATCommandAndCheckReply(NULL, "Call Ready", configTICK_RATE_HZ * 20)) {
		printf("Wait Call Realy timeout\n");
	}

	if (!ATCommandAndCheckReply("ATS0=3\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("ATS0=3 error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CMGF=0\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+CMGF=0 error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CNMI=2,2,0,0,0\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+CNMI error\r");
		return false;
	}

	if (!ATCommandAndCheckReplyUntilOK("AT+CPMS=\"SM\"\r", "+CPMS", configTICK_RATE_HZ * 3, 10)) {
		printf("AT+CPMS error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CMGD=1,4\r", "OK", configTICK_RATE_HZ * 5)) {
		printf("AT+CMGD error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CSQ\r", "+CSQ:", configTICK_RATE_HZ / 5)) {
		printf("AT+CSQ error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QIDEACT\r", "DEACT", configTICK_RATE_HZ / 5)) {
		printf("AT+QIDEACT error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QIHEAD=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIHEAD error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QISHOWRA=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWRA error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QISHOWPT=1\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWPT error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QIFGCNT=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIFGCNT error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QICSGP=1,\"CMNET\"\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QICSGP error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QIMUX=0\r", "OK", configTICK_RATE_HZ)) {
		printf("AT+QIMUX error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT&W\r", "OK", configTICK_RATE_HZ)) {
		printf("AT&W error\r");
		return false;
	}
	return true;
}

void __handleSMS(GsmTaskMessage *p) {
	SMSInfo *sms;
	const char *dat = __gsmGetMessageData(p);
	sms = __gsmPortMalloc(sizeof(SMSInfo));
	printf("Gsm: got sms => %s\n", dat);
	SMSDecodePdu(dat, sms);
	__gsmSMSEncodeConvertToGBK(sms);
	printf("Gsm: sms_content=> %s\n", sms->content);
#if defined(__SPEAKER__)
	XfsTaskSpeakGBK(sms->content, sms->content_len);
#elif defined(__LED__)
	ProtocolHandlerSMS(sms);
#endif
	__gsmPortFree(sms);
}

bool __gsmIsValidImei(const char *p) {
	int i;
	if (strlen(p) != GSM_IMEI_LENGTH) {
		return false;
	}

	for (i = 0; i < GSM_IMEI_LENGTH; i++) {
		if (!isdigit(p[i])) {
			return false;
		}
	}

	return true;
}

int __gsmGetImeiFromModem() {
	int i;
	char *reply;
	reply = ATCommand("AT+GSN\r", ATCMD_ANY_REPLY_PREFIX, configTICK_RATE_HZ / 10);
	if (reply == NULL) {
		return 0;
	}
	if (!__gsmIsValidImei(reply)) {
		return 0;
	}
	strcpy(__imei, reply);
	for (i = 0; i < 15; i++) {
		__gsmRuntimeParameter.IMEI[i] = __imei[i];
	}
	__storeGsmRuntimeParameter();
	AtCommandDropReplyLine(reply);
	return 1;
}

void __handleProtocol(GsmTaskMessage *msg) {
	ProtocolHandler(__gsmGetMessageData(msg));
}

void __handleSendTcpDataLowLevel(GsmTaskMessage *msg) {
	__gsmSendTcpDataLowLevel(__gsmGetMessageData(msg), msg->length);
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


void __handleSendAtCommand(GsmTaskMessage *msg) {
	ATCommand(__gsmGetMessageData(msg), NULL, configTICK_RATE_HZ / 10);
}

void __handleSendSMS(GsmTaskMessage *msg) {
	static const char *hexTable = "0123456789ABCDEF";
	char buf[16];
	int i;
	char *p = __gsmGetMessageData(msg);
	sprintf(buf, "AT+CMGS=%d\r", msg->length - 1);
	ATCommand(buf, NULL, configTICK_RATE_HZ / 5);
	for (i = 0; i < msg->length; ++i) {
		ATCmdSendChar(hexTable[*p >> 4]);
		ATCmdSendChar(hexTable[*p & 0x0F]);
		++p;
	}
	ATCmdSendChar(0x1A);

	p = ATCommand(NULL, "OK", configTICK_RATE_HZ * 15);
	if (p != NULL) {
		AtCommandDropReplyLine(p);
		printf("Send SMS OK.\n");
	} else {
		printf("Send SMS error.\n");
	}
}

typedef struct {
	GsmTaskMessageType type;
	void (*handlerFunc)(GsmTaskMessage *);
} MessageHandlerMap;

static const MessageHandlerMap __messageHandlerMaps[] = {
	{ TYPE_SMS_DATA, __handleSMS },
	{ TYPE_RING, __handleRING },
	{ TYPE_GPRS_DATA, __handleProtocol },
	{ TYPE_SEND_TCP_DATA, __handleSendTcpDataLowLevel },
	{ TYPE_RESET, __handleReset },
	{ TYPE_NO_CARRIER, __handleResetNoCarrier },
	{ TYPE_SEND_AT, __handleSendAtCommand },
	{ TYPE_SEND_SMS, __handleSendSMS },
	{ TYPE_NONE, NULL },
};

static void __gsmTask(void *parameter) {
	portBASE_TYPE rc;
	GsmTaskMessage *message;
	portTickType lastT = 0;

	while (1) {
		printf("Gsm start\n");
		__gsmModemStart();
		if (__initGsmRuntime()) {
			break;
		}
	}

	while (!__gsmGetImeiFromModem()) {
		vTaskDelay(configTICK_RATE_HZ);
	}

	for (;;) {
		printf("Gsm: loop again\n");
		rc = xQueueReceive(__queue, &message, configTICK_RATE_HZ * 10);
		if (rc == pdTRUE) {
			const MessageHandlerMap *map = __messageHandlerMaps;
			for (; map->type != TYPE_NONE; ++map) {
				if (message->type == map->type) {
					map->handlerFunc(message);
					break;
				}
			}
			__gsmDestroyMessage(message);
		} else {
			int curT = xTaskGetTickCount();
			if (0 == __gsmCheckTcpAndConnect(__gsmRuntimeParameter.serverIP, __gsmRuntimeParameter.serverPORT)) {
				printf("Gsm: Connect TCP error\n");
			} else if ((curT - lastT) >= GSM_GPRS_HEART_BEAT_TIME) {
				int size;
				const char *dat = ProtoclCreateHeartBeat(&size);
				__gsmSendTcpDataLowLevel(dat, size);
				ProtocolDestroyMessage(dat);
				lastT = curT;
			}
		}
	}
}


void GSMInit(void) {
	ATCommandRuntimeInit();
	__gsmInitHardware();
	__queue = xQueueCreate(5, sizeof(GsmTaskMessage *));
	xTaskCreate(__gsmTask, (signed portCHAR *) "GSM", GSM_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
}
