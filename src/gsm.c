#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include "rtc.h"
#include "gsm.h"
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
#include "soundcontrol.h"
#include "second_datetime.h"

#define GSM_TASK_STACK_SIZE			     (configMINIMAL_STACK_SIZE + 256)
#define GSM_GPRS_HEART_BEAT_TIME     (configTICK_RATE_HZ * 60 * 5)
#define GSM_IMEI_LENGTH              15

#define  RING_PIN  GPIO_Pin_15

#if defined(__SPEAKER_V1__)
#  define RESET_GPIO_GROUP           GPIOA
#  define RESET_GPIO                 GPIO_Pin_11
#elif defined(__SPEAKER_V2__)|| defined (__SPEAKER_V3__)
#  define RESET_GPIO_GROUP           GPIOG
#  define RESET_GPIO                 GPIO_Pin_10
#elif defined(__LED__)
#  define RESET_GPIO_GROUP           GPIOB
#  define RESET_GPIO                 GPIO_Pin_1
#endif

#define __gsmAssertResetPin()        GPIO_SetBits(RESET_GPIO_GROUP, RESET_GPIO)
#define __gsmDeassertResetPin()      GPIO_ResetBits(RESET_GPIO_GROUP, RESET_GPIO)

#if defined (__SPEAKER_V3__)
#define __gsmPowerSupplyOn()         GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define __gsmPowerSupplyOff()        GPIO_SetBits(GPIOB, GPIO_Pin_0)

#else
#define __gsmPowerSupplyOn()         GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define __gsmPowerSupplyOff()        GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#endif

#define __gsmPortMalloc(size)        pvPortMalloc(size)
#define __gsmPortFree(p)             vPortFree(p)


XFSspeakParam  __speakParam;

void __gsmSMSEncodeConvertToGBK(SMSInfo *info ) {
	uint8_t *gbk;

	if (info->encodeType == ENCODE_TYPE_GBK) {
		return;
	}
	gbk = Unicode2GBK((const uint8_t *)info->content, info->contentLen);
	strcpy((char *)info->content, (const char *)gbk);
	Unicode2GBKDestroy(gbk);
	info->encodeType = ENCODE_TYPE_GBK;
	info->contentLen = strlen((const char *)info->content);
}


/// GSM task message queue.
static xQueueHandle __queue;

/// Save the imei of GSM modem, filled when GSM modem start.
static char __imei[GSM_IMEI_LENGTH + 1];

const char *GsmGetIMEI(void) {
	return __imei;
}

/// Save runtime parameters for GSM task;
//static GMSParameter __gsmRuntimeParameter = {"61.190.61.78", 5555, 1, 0, "0620", 1, 1};	  // 老平台服务器及端口："221.130.129.72",5555

static GMSParameter __gsmRuntimeParameter = {"221.130.129.72", 5555, 1, 0, "0620", 1, 1};

/// Basic function for sending AT Command, need by atcmd.c.
/// \param  c    Char data to send to modem.
void ATCmdSendChar(char c) {
	USART_SendData(USART2, c);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

//Store __gsmRuntimeParameter to flash.
static inline void __storeGsmRuntimeParameter(void) {
	NorFlashWrite(GSM_PARAM_STORE_ADDR, (const short *)&__gsmRuntimeParameter, sizeof(__gsmRuntimeParameter));
}

//Restore __gsmRuntimeParameter from flash.
static inline void __restorGsmRuntimeParameter(void) {
	NorFlashRead(GSM_PARAM_STORE_ADDR, (short *)&__gsmRuntimeParameter, sizeof(__gsmRuntimeParameter));
}

static char GSM_FLAG = 0;

static inline void __storeFLAGParameter(void) {
	NorFlashWrite(FLAG_PARAM_STORE_ADDR, (const short *)&GSM_FLAG, 1);
}

//Restore __gsmRuntimeParameter from flash.
static inline void __restorFLAGParameter(void) {
	NorFlashRead(FLAG_PARAM_STORE_ADDR, (short *)&GSM_FLAG, 1);
}

/// Low level set TCP server IP and port.
//static void __setGSMserverIPLowLevel(char *ip, int port) {
//	strcpy(__gsmRuntimeParameter.serverIP, ip);
//	__gsmRuntimeParameter.serverPORT = port;
//	__storeGsmRuntimeParameter();
//}

typedef enum {
	TYPE_NONE = 0,
	TYPE_SMS_DATA,
	TYPE_RING,
	TYPE_GPRS_DATA,
	TYPE_RTC_DATA,
	TYPE_TUDE_DATA,
	TYPE_CSQ_DATA,
	TYPE_SEND_TCP_DATA,
	TYPE_RESET,
	TYPE_NO_CARRIER,
	TYPE_SEND_AT,
	TYPE_SEND_SMS,
	TYPE_SET_GPRS_CONNECTION,
	TYPE_SETIP,
	TYPE_SETSPACING,
	TYPE_SETFREQU,
	TYPE_HTTP_DOWNLOAD,
	TYPE_SET_NIGHT_QUIET,
	TYPE_QUIET_TIME,
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
    GsmTaskMessage *message;
    if(strncasecmp((const char *)pdu, "<SETIP>", 7) == 0){
	   message = __gsmCreateMessage(TYPE_SETIP, &pdu[7], len-7);
	} else if (strncasecmp((const char *)pdu, "<QUIET>", 7) == 0){
	   message = __gsmCreateMessage(TYPE_QUIET_TIME, &pdu[9], len-9);
	} else if (strncasecmp((const char *)pdu, "<SPA>", 5) == 0){
	   message = __gsmCreateMessage(TYPE_SETSPACING, &pdu[5], len-5);
	} else if (strncasecmp((const char *)pdu, "<FREQ>", 6) == 0){
	   message = __gsmCreateMessage(TYPE_SETFREQU, &pdu[6], len-6);
	} else {
	   message = __gsmCreateMessage(TYPE_SEND_SMS, pdu, len);
	}
//	char *dat = __gsmGetMessageData(message);
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 15)) {
		__gsmDestroyMessage(message);
		return false;
	}
	return true;
}


bool GsmTaskSetParameter(const char *dat, int len) {
	GsmTaskMessage *message;
	  if(strncasecmp((const char *)dat, "<CTCP>", 6) == 0){
	   message = __gsmCreateMessage(TYPE_SET_GPRS_CONNECTION, &dat[6], 1);
	} else if(strncasecmp((const char *)dat, "<QUIET>", 7) == 0){
	   message = __gsmCreateMessage(TYPE_SET_NIGHT_QUIET, &dat[7], 1);
	}
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
	GsmTaskMessage *message;
	if(strncasecmp((const char *)dat, "<http>", 6) == 0){
	    message = __gsmCreateMessage(TYPE_HTTP_DOWNLOAD, &dat[6], (len - 6));
	} else {	
	    message = __gsmCreateMessage(TYPE_SEND_TCP_DATA, dat, len);
	}
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 15)) {
		__gsmDestroyMessage(message);
		return true;
	}
	return false;
}

static void __gsmInitUsart(int baud) {
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = baud;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART2, ENABLE);
}

/// Init the CPU on chip hardware for the GSM modem.
static void __gsmInitHardware(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);				   //GSM模块的串口

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

#if defined(__SPEAKER_V3__)
	GPIO_ResetBits(GPIOG, GPIO_Pin_14);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOG, &GPIO_InitStructure);	               //SMS到来标志位

    GPIO_ResetBits(GPIOG, RING_PIN);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOG, &GPIO_InitStructure);				   //RING到来标志位

	GPIO_SetBits(GPIOD, GPIO_Pin_3);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);				   //判断TCP是否打开
#endif

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



static char buffer[4096];
static int bufferIndex = 0;
static char isIPD = 0;
static char isSMS = 0;
static char isRTC = 0;
static char isTUDE = 0;
static char isCSQ = 0;
static char CELLloc = 0;
static char GSMloc = 0;
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
		if (pdTRUE == xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				taskYIELD();
			}
		}
		isSMS = 0;
		bufferIndex = 0;
	} else if (data != 0x0D) {
		buffer[bufferIndex++] = data;
	}
}

static inline void __gmsReceiveRTCData(unsigned char data) {
	if (data == 0x0A) {
		GsmTaskMessage *message;
		portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
		buffer[bufferIndex++] = 0;
		message = __gsmCreateMessage(TYPE_RTC_DATA, buffer, bufferIndex);
		if (pdTRUE == xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				taskYIELD();
			}
		}
		isRTC = 0;
		bufferIndex = 0;
	} else if (data != 0x0D) {
		buffer[bufferIndex++] = data;
	}
}

static inline void __gmsReceiveTUDEData(unsigned char data) {
	if (data == 0x0A) {
		GsmTaskMessage *message;
		portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
		buffer[bufferIndex++] = 0;
		message = __gsmCreateMessage(TYPE_TUDE_DATA, buffer, bufferIndex);
		if (pdTRUE == xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				taskYIELD();
			}
		}
		isTUDE = 0;
		bufferIndex = 0;
	} else if (data != 0x0D) {
		buffer[bufferIndex++] = data;
	}
}

static inline void __gmsReceiveCSQData(unsigned char data) {
	if (data == 0x0A) {
		GsmTaskMessage *message;
		portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
		buffer[bufferIndex++] = 0;
		message = __gsmCreateMessage(TYPE_CSQ_DATA, buffer, bufferIndex);
		if (pdTRUE == xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				taskYIELD();
			}
		}
		isCSQ = 0;
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

	if (isRTC) {
		__gmsReceiveRTCData(data);
		return;
	}
	
	if (isTUDE) {
		__gmsReceiveTUDEData(data);
		return;
	}
	
	if (isCSQ) {
		__gmsReceiveCSQData(data);
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
		if (strncmp(buffer, "+QNITZ: ", 8) == 0) {
			bufferIndex = 0;
			isRTC = 1;
		}
		
		if (strncmp(buffer, "+CSQ: ", 6) == 0) {
			bufferIndex = 0;
			isCSQ = 1;
		}
		
		if (strncmp(buffer, "+QCELLLOC: ", 11) == 0) {
			bufferIndex = 0;
			isTUDE = 1;
			CELLloc = 1;
		}
		
		if (strncmp(buffer, "+QGSMLOC: ", 10) == 0) {
			bufferIndex = 0;
			isTUDE = 1;
			GSMloc = 1;
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

static char Count = 0;
static unsigned char Dflag = 0;

bool __gsmIsTcpConnected() {
	char *reply;
	unsigned char k = 0;
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
			vTaskDelay(configTICK_RATE_HZ * 5);
			k++;
			if(k > 12) {
				Dflag = 1;
				return false;
			}
			continue;
		}
		if (strncmp(&reply[7], "IP CLOSE", 8) == 0) {
			AtCommandDropReplyLine(reply);
			Count++;
			if(Count > 5) {
				Dflag = 1;
			  Count = 0;
			}
			return false;
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

	sprintf(buf, "AT+QISEND=%d\r", len);		  //len多大1460
	ATCommand(buf, NULL, configTICK_RATE_HZ / 2);
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
	int i;
	static const int bauds[] = {19200, 9600, 115200, 38400, 57600, 4800};
	for (i = 0; i < ARRAY_MEMBER_NUMBER(bauds); ++i) {
		// 设置波特率
		printf("Init gsm baud: %d\n", bauds[i]);
		__gsmInitUsart(bauds[i]);
		ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ );
		ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ );

		if (ATCommandAndCheckReply("ATE0\r", "OK", configTICK_RATE_HZ * 2)) {
			break;
		}
	}
	if (i >= ARRAY_MEMBER_NUMBER(bauds)) {
		printf("All baud error\n");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+IPR=19200\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+IPR=19200 error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QNITZ=1\r", "OK", configTICK_RATE_HZ)) {
		printf("AT+IFC error\r");
		return false;
	}

// 	if (!ATCommandAndCheckReply("AT+IFC=2,2\r", "OK", configTICK_RATE_HZ)) {
// 		printf("AT+QNITZ error\r");
// 		return false;
// 	}

	if (!ATCommandAndCheckReply(NULL, "Call Ready", configTICK_RATE_HZ * 30)) {
		printf("Wait Call Realy timeout\n");
	}

	if (!ATCommandAndCheckReply("ATS0=5\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("ATS0=5 error\r");
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

	if (!ATCommandAndCheckReplyUntilOK("AT+CPMS=\"SM\"\r", "+CPMS", configTICK_RATE_HZ * 10, 3)) {
		printf("AT+CPMS error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CMGD=1,4\r", "OK", configTICK_RATE_HZ * 5)) {
		printf("AT+CMGD error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CSQ\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+CSQ error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+QIDEACT\r", "DEACT", configTICK_RATE_HZ / 5)) {
		printf("AT+QIDEACT error\r");
		return false;
	}		   //关闭GPRS场景

	if (!ATCommandAndCheckReply("AT+QIHEAD=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIHEAD error\r");
		return false;
	}		   //配置接受数据时是否显示IP头

	if (!ATCommandAndCheckReply("AT+QISHOWRA=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWRA error\r");
		return false;
	}		  //配置接受数据时是否显示发送方的IP地址和端口号

	if (!ATCommandAndCheckReply("AT+QISHOWPT=1\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWPT error\r");
		return false;
	}		   //配置接受数据IP头是否显示传输协议

	if (!ATCommandAndCheckReply("AT+QIFGCNT=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIFGCNT error\r");
		return false;
	}			//配置前置场为GPRS

	if (!ATCommandAndCheckReply("AT+QICSGP=1,\"CMNET\"\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QICSGP error\r");
  		return false;
	}			//打开GPRS连接

#if defined (__SPEAKER_V1__)
	if (!ATCommandAndCheckReply("AT+QCELLLOC=1\r", "OK", configTICK_RATE_HZ * 10)) {
		printf("AT+QCELLLOC error\r");
	}
#endif

// 	if (!ATCommandAndCheckReply("AT+QGSMLOC=1\r", "OK", configTICK_RATE_HZ * 10)) {
// 		printf("AT+QGSMLOC error\r");
// 	}
	
	printf("GSM init OK.\r");
	return true;
}

//void chooseTCP(bool state){
//	__gsmRuntimeParameter.isonTCP = state;
//    __storeGsmRuntimeParameter();
//	if(state == 0){
//	   	if (!ATCommandAndCheckReply("AT+QIDEACT\r", "DEACT", configTICK_RATE_HZ / 2)) {
//		     printf("AT+QIDEACT error\r");
//	    }
//	}
//}
static char FlagII = 0;
static char FlagIII = 0;
static char repeat = 0;
extern int *oflen(void);

void __handleSMS(GsmTaskMessage *p) {
	SMSInfo *sms;
	uint32_t second;
	DateTime dateTime;
	char *pcontent;
	const char *dat = __gsmGetMessageData(p);
	if(__gsmRuntimeParameter.isonQUIET){
	   second = RtcGetTime();
	   SecondToDateTime(&dateTime, second);
	   if((dateTime.hour < ((__gsmRuntimeParameter.time[0] - '0') * 10 + (__gsmRuntimeParameter.time[1] - '0'))) ||
	      (dateTime.hour > ((__gsmRuntimeParameter.time[2] - '0') * 10 + (__gsmRuntimeParameter.time[3] - '0')))){
			if(FlagII == 0){	
	   	  XfsTaskSetSpeakVolume('0');
				FlagII = 1;
				FlagIII = 0;
      }	
	   } else {
			 if(FlagIII == 0){
	   	   XfsTaskSetSpeakVolume('9');
				 FlagII = 0;
				 FlagIII = 1;
			 }
	   }
	}
	sms = __gsmPortMalloc(sizeof(SMSInfo));
	printf("Gsm: got sms => %s\n", dat);
	SMSDecodePdu(dat, sms);
	printf("Gsm: sms_content=> %s\n", sms->content);
	if(sms->contentLen == 0) {
		__gsmPortFree(sms);
		return;
	}		
  
	pcontent = sms->content;
	*oflen() = 0;
	if(pcontent[0] != '<'){
		repeat = 1;
	}
	ProtocolHandlerSMS(sms);
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
	char *reply;	
	reply = ATCommand("AT+GSN\r", ATCMD_ANY_REPLY_PREFIX, configTICK_RATE_HZ / 2);
	if (reply == NULL) {
		return 0;
	}
	if (!__gsmIsValidImei(reply)) {
		return 0;
	}
	strcpy(__imei, reply);
	AtCommandDropReplyLine(reply);
	return 1;
}

void __handleProtocol(GsmTaskMessage *msg) {
	ProtocolHandler(__gsmGetMessageData(msg));
}

void __handleSendTcpDataLowLevel(GsmTaskMessage *msg) {
	__gsmSendTcpDataLowLevel(__gsmGetMessageData(msg), msg->length);
}

static char mountTime[30] = {0};

void __handleM35RTC(GsmTaskMessage *msg) {
	DateTime dateTime;
	char *p = __gsmGetMessageData(msg);	 
	p++;
	sprintf(mountTime, p);
	dateTime.year = (p[0] - '0') * 10 + (p[1] - '0');
	dateTime.month = (p[3] - '0') * 10 + (p[4] - '0');
	dateTime.date = (p[6] - '0') * 10 + (p[7] - '0');
	dateTime.hour = (p[9] - '0' + 8) * 10 + (p[10] - '0');
	dateTime.minute = (p[12] - '0') * 10 + (p[13] - '0');
	dateTime.second = (p[15] - '0') * 10 + (p[16] - '0');
	RtcSetTime(DateTimeToSecond(&dateTime));
}

char *fix(void){
	return &mountTime[0];
}

static char longitude[12] = {1}, latitude[12] = {1};

void __handleTUDE(GsmTaskMessage *msg) {
	int i, j = 0, count = 0;
	char date[12], time[10];
	DateTime __dateTime;
  char *p = __gsmGetMessageData(msg);
	if (GSMloc == 1){
			*p++;
			*p++;
			for(i = 0; i < 4; i++){
					count++;
					while(*p != ','){
						if (count == 1){
							longitude[j++] = *p++;
						}	else if (count == 2){ 
							latitude[j++] = *p++;
						}	else if (count == 3){ 
							date[j++] = *p++;
						}	else if (count == 4){					
								time[j++] = *p++;
							if(*p == 0){
								break;
							}
						}	
					}
					*p++;
					j = 0;
			}
			__dateTime.year = (date[2] - '0') * 10 + (date[3] - '0');
			__dateTime.month = (date[5] - '0') * 10 + (date[6] - '0');
			__dateTime.date = (date[8] - '0') * 10 + (date[9] - '0');
			__dateTime.hour = (time[0] - '0') * 10 + (time[1] - '0') + 8;
			__dateTime.minute = (time[3] - '0') * 10 + (time[4] - '0');
			__dateTime.second = (time[6] - '0') * 10 + (time[7] - '0');
			RtcSetTime(DateTimeToSecond(&__dateTime));
			GSMloc = 0;
		}
		
		if(CELLloc == 1) {
				for(i = 0; i < 2; i++){
						count++;
						while(*p != ','){
							if (count == 1){
								longitude[j++] = *p++;
							}	else if (count == 2){ 
								latitude[j++] = *p++;
								if(*p == 0){
									break;
								}
							}	
						}
						*p++;
						j = 0;
		    }
				CELLloc = 0;
		}
}

const char *__gsmGetTUDE(char *p) {
	p = pvPortMalloc(30);
	memset(p, 0, 30);
	strcpy(p, longitude);
  strcat(p, ",");
	strcat(p, latitude);
	return p;
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
	SoundControlSetChannel(SOUND_CONTROL_CHANNEL_GSM, 0);
	GPIO_SetBits(GPIOG, RING_PIN);
}

void __handleRING(GsmTaskMessage *msg) {
	SoundControlSetChannel(SOUND_CONTROL_CHANNEL_GSM, 1);
	GPIO_ResetBits(GPIOG, RING_PIN);
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


void __handleGprsConnection(GsmTaskMessage *msg) {	
	char *dat = __gsmGetMessageData(msg);
	__gsmRuntimeParameter.isonTCP = (*dat != 0x30);
    __storeGsmRuntimeParameter();
	if(!__gsmRuntimeParameter.isonTCP){
	   	if (!ATCommandAndCheckReply("AT+CGATT=0\r", "OK", configTICK_RATE_HZ )) {
		     printf("AT+CGATT error\r");
	    }
	}
}

//<SETIP>"221.130.129.72"5555
void __handleSetIP(GsmTaskMessage *msg) {
   int j = 0;
   char *dat = __gsmGetMessageData(msg);
	 if (!ATCommandAndCheckReply("AT+QICLOSE\r", "CLOSE OK", configTICK_RATE_HZ / 2)) {
		 printf("AT+QICLOSE error\r");
	 }	
	 memset(__gsmRuntimeParameter.serverIP, 0, 16);
	 if(*dat++ == 0x22){
		while(*dat != 0x22){
			 __gsmRuntimeParameter.serverIP[j++] = *dat++;
		}
		*dat++;
	 }
	 __gsmRuntimeParameter.serverPORT = atoi(dat);
	 __storeGsmRuntimeParameter();
}

void __handleSetSpacing(GsmTaskMessage *msg) {
	char *dat = __gsmGetMessageData(msg);
	if((msg->length) == 1){
	  __gsmRuntimeParameter.spacing = *dat - '0';
	} else if((msg->length) >= 2){
		__gsmRuntimeParameter.spacing = (*dat++ - '0') * 10 + (*dat - '0');
	}
	__storeGsmRuntimeParameter();
}

void __handleSetFrequ(GsmTaskMessage *msg) {
	char *dat = __gsmGetMessageData(msg);
	if((msg->length) == 1){
	  __gsmRuntimeParameter.frequency = *dat - '0';
	} else if((msg->length) >= 2){
		__gsmRuntimeParameter.frequency = (*dat++ - '0') * 10 + (*dat - '0');
	}
	__storeGsmRuntimeParameter();
}

void __handleNightQuiet(GsmTaskMessage *msg) {
    char *dat = __gsmGetMessageData(msg);
	__gsmRuntimeParameter.isonQUIET = (*dat != 0x30);
	__storeGsmRuntimeParameter();
}

static unsigned char Vcsq = 0;

void __handleCSQ(GsmTaskMessage *msg) {
    char *dat = __gsmGetMessageData(msg);
	  if (dat[0] == 0x20) {
			*dat++;
		}
	
	  if ((dat[0] > 0x33) && (dat[0] < 0x30)){
			Vcsq = 8;
			return;
		}
		
    Vcsq = (dat[0] - '0') * 10 + dat[1] - '0';
		
	  if (Vcsq > 31) {
			Vcsq = 10;
		}
}


void __handleQuietTime(GsmTaskMessage *msg) {
     int i;
	 char *dat = __gsmGetMessageData(msg);
	 for(i = 0; i < 4; i++){
	 	 __gsmRuntimeParameter.time[i] = *dat++;
	 }
	 //__gsmRuntimeParameter.time = *dat;
	 __storeGsmRuntimeParameter();
}

void __handleHttpDownload(GsmTaskMessage *msg) {
    char buf[44];
    char *dat = __gsmGetMessageData(msg);
	char *pref = pvPortMalloc(100);
	strcpy(pref, "http://");
	strcat(pref, dat);
	sprintf(buf, "AT+QHTTPURL=%d,35\r", strlen(pref));

	if (!ATCommandAndCheckReply("AT+QIFGCNT=1\r", "OK", configTICK_RATE_HZ / 2)) {
		printf("AT+QIFGCNT error\r");
		return;
	}	

	if (!ATCommandAndCheckReply(buf, "CONNECT", configTICK_RATE_HZ * 6)) {
		printf("AT+QHTTPURL error\r");
		vPortFree(pref);
  		return;
	}

	if (!ATCommandAndCheckReply(pref, "OK", configTICK_RATE_HZ)) {
		printf("URL error\r");
		vPortFree(pref);
  		return;
	}

	if(!ATCommandAndCheckReply("AT+QHTTPGET=60\r", "OK", configTICK_RATE_HZ * 5 )) {
		printf("AT+QHTTPGET error\r");
		vPortFree(pref);
  		return;
	}					// 发送HTTP获得资源请求

	if (!ATCommandAndCheckReply("AT+QHTTPREAD=30\r", "OK", configTICK_RATE_HZ * 10)) {
		printf("AT+QHTTREAD error\r");
		vPortFree(pref);
  		return;
	}

	vPortFree(pref);
	
	if (!ATCommandAndCheckReply("AT+QIFGCNT=0\r", "OK", configTICK_RATE_HZ / 2)) {
		printf("AT+QIFGCNT error\r");
		return;
	}			//配置前置场为GPRS	
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
	{ TYPE_RTC_DATA, __handleM35RTC},
	{ TYPE_RESET, __handleReset },
	{ TYPE_NO_CARRIER, __handleResetNoCarrier },
  { TYPE_CSQ_DATA, __handleCSQ },
	{ TYPE_SEND_AT, __handleSendAtCommand },
	{ TYPE_SEND_SMS, __handleSendSMS },
  { TYPE_TUDE_DATA, __handleTUDE},
	{ TYPE_SET_GPRS_CONNECTION, __handleGprsConnection },
	{ TYPE_SETIP, __handleSetIP },
  { TYPE_SETSPACING, __handleSetSpacing },
  { TYPE_SETFREQU, __handleSetFrequ },
	{ TYPE_HTTP_DOWNLOAD, __handleHttpDownload },
	{ TYPE_SET_NIGHT_QUIET, __handleNightQuiet },
	{ TYPE_QUIET_TIME, __handleQuietTime},
	{ TYPE_NONE, NULL },
};

extern char *playOff();

static void __gsmTask(void *parameter) {
	portBASE_TYPE rc;
	GsmTaskMessage *message;
	portTickType lastT = 0, realT = 0, recT = 0, repT = 0xFFFFFFFF;
	int i = 0;
	const char *t = (const char *)(Bank1_NOR2_ADDR + FLAG_PARAM_STORE_ADDR);
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
	
	__restorFLAGParameter();
  	
	if(*t == 0xff){
		GSM_FLAG = 1;
		__storeFLAGParameter();
		__storeGsmRuntimeParameter();
	} else {
		__restorGsmRuntimeParameter();
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
			portTickType curT;
			
			curT = xTaskGetTickCount();		
			if(repeat == 1) {
				if((*playOff()) == 1){
					*playOff() = 0;
					repT = curT;
				}
				
				if(repT <= curT){
					if((curT - repT) >= GSM_GPRS_HEART_BEAT_TIME * (__gsmRuntimeParameter.spacing))	{
						const char *t = (const char *)(Bank1_NOR2_ADDR + SMS1_PARAM_STORE_ADDR);
						i++;
						if(i >= (__gsmRuntimeParameter.frequency)){
							repeat = 0;
							i = 0;
							repT = 0xFFFFFFFF;
						} else {
							XfsTaskSpeakUCS2(t, *oflen());
							repT = curT;
						}
					}	
				}
			}
			
			if(__gsmRuntimeParameter.isonTCP == 0){
			   continue;
			}								
			
			if ((curT - recT) >= GSM_GPRS_HEART_BEAT_TIME * 12) {
				Dflag = 0;
				recT = curT;
			}
			
			if(Dflag == 1) {				
				continue;
			}
			
			if(Vcsq == 0) {
				continue;
			}
			
			if ((curT - realT) >= GSM_GPRS_HEART_BEAT_TIME * 6) {
			   GsmTaskSendAtCommand("AT+CSQ");
				 realT = curT;
			}			
			
			if (0 == __gsmCheckTcpAndConnect(__gsmRuntimeParameter.serverIP, __gsmRuntimeParameter.serverPORT)) {
				Count++;
				if(Count > 5) {
					Dflag = 1;
					Count = 0;
					continue;
				}
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

unsigned char *Gsmpara(unsigned char *p){
	unsigned char len;
	p = pvPortMalloc(64);
	len = sprintf(p, "{\"%s\",", __gsmRuntimeParameter.serverIP);
	len += sprintf(&p[len], "%d,", __gsmRuntimeParameter.serverPORT);
	len += sprintf(&p[len], "%d,", __gsmRuntimeParameter.isonTCP);
	len += sprintf(&p[len], "%d,", __gsmRuntimeParameter.isonQUIET);
	len += sprintf(&p[len], "%s}", __gsmRuntimeParameter.time);
	len += sprintf(&p[len], "#C%d", Vcsq);
	return p;
}

void GSMInit(void) {
	ATCommandRuntimeInit();
	__gsmInitHardware();
	__queue = xQueueCreate(5, sizeof( GsmTaskMessage *));
	xTaskCreate(__gsmTask, (signed portCHAR *) "GSM", GSM_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
}
