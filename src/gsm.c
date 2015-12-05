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
#include "stm32f10x_exti.h"
#include "protocol.h"
#include "misc.h"
#include "sms.h"
#include "zklib.h"
#include "atcmd.h"
#include "norflash.h"
#include "second_datetime.h"

#define BROACAST   "9999999999"

#define GSM_TASK_STACK_SIZE			     (configMINIMAL_STACK_SIZE + 512)

#define __gsmPortMalloc(size)        pvPortMalloc(size)
#define __gsmPortFree(p)             vPortFree(p)

#define  SerX                USART3

#define  GPIO_GSM           GPIOB
#define  Pin_Restart        GPIO_Pin_0
#define  Pin_Reset	        GPIO_Pin_2

#define  GPIO_GPRS_PW_EN    GPIOB
#define  PIN_GPRS_PW_EN     GPIO_Pin_7

#define  RELAY_SWITCH_GPIO  GPIOB
#define  RELAY_SWITCH_PIN   GPIO_Pin_12

#define  RELAY_EXTI         EXTI15_10_IRQn

static xQueueHandle __queue;

static GMSParameter __gsmRuntimeParameter = {"0551010006", "61.190.38.46", 10000};	  // 老平台服务器及端口："221.130.129.72",5555

void ATCmdSendChar(char c) {
	USART_SendData(SerX, c);
	while (USART_GetFlagStatus(SerX, USART_FLAG_TXE) == RESET);
}

typedef enum {
	TYPE_NONE = 0,
	TYPE_SMS_DATA,
	TYPE_GPRS_DATA,
	TYPE_RTC_DATA,
	TYPE_SEND_TCP_DATA,
	TYPE_COPS_DATA,
	TYPE_CSQ_DATA,
	TYPE_SEND_AT,
	TYPE_SEND_SMS,
} GsmTaskMessageType;

typedef struct {
	GsmTaskMessageType type;
	unsigned char length;
	char GSMmsg[256];
} GsmTaskMessage;

static inline void *__gsmGetMessageData(GsmTaskMessage *message) {
	return &message[1];
}

GsmTaskMessage *__gsmCreateMessage(GsmTaskMessageType type, const char *dat, unsigned char len) {
	GsmTaskMessage *message = __gsmPortMalloc(ALIGNED_SIZEOF(GsmTaskMessage) + len + 1);
	if (message != NULL) {
		message->type = type;
		message->length = len;
		memcpy(&message[1], dat, len);
		*((char *)message + ALIGNED_SIZEOF(GsmTaskMessage) + len) = 0;
	}
	return message;
}

void __gsmDestroyMessage(GsmTaskMessage *message) {
	__gsmPortFree(message);
}

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

bool GsmTaskSendTcpData(const char *dat, unsigned char len) {

	GsmTaskMessage *message;
	message = __gsmCreateMessage(TYPE_SEND_TCP_DATA, dat, len);
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 15)) {
		__gsmDestroyMessage(message);
		return true;
	}
	return false;
}

bool GsmTaskSendSMS(const char *pdu, int len) {
  GsmTaskMessage *message;

	message = __gsmCreateMessage(TYPE_SEND_SMS, pdu, len);
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 15)) {
		__gsmDestroyMessage(message);
		return false;
	}
	return true;
}

static void __gsmInitUsart(int baud) {
	USART_InitTypeDef USART_InitStructure;
	
	USART_InitStructure.USART_BaudRate = baud;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(SerX, &USART_InitStructure);
	USART_ITConfig(SerX, USART_IT_RXNE, ENABLE);
	
	USART_Cmd(SerX, ENABLE);
}

static void __gsmInitHardware(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);				   //GSM模块的串口

  GPIO_SetBits(GPIO_GSM, Pin_Reset);
	GPIO_ResetBits(GPIO_GSM, Pin_Restart);

  GPIO_InitStructure.GPIO_Pin =  Pin_Restart | Pin_Reset;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_GSM, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin =  PIN_GPRS_PW_EN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_GPRS_PW_EN, &GPIO_InitStructure);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
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
	{ NULL, TYPE_NONE },
};

static char CMCC = 0;   //是否为中国移动, 1为中国移动，2为中国联通

char *isCMCC(void){
	return &CMCC;
}

static char buffer[255];
static char bufferIndex = 0;
static char isIPD = 0;
static char isRTC = 0;
static char isCops = 0;
static char SENDERROE = 0;
static unsigned char lenIPD = 0;
static char TcpConnect = 0;
static char isCSQ = 0;

char __sendstatus(char tmp){
	if (tmp == 0){
		SENDERROE = 0;
	}
	return SENDERROE;
}

static inline void __gmsReceiveIPDData(unsigned char data) {
	char param1, param2;
	
	buffer[bufferIndex++] = data;
	if(bufferIndex == 15) {
		if (buffer[13] > '9') {
			param1 = buffer[13] - '7';
		} else {
			param1 = buffer[13] - '0';
		}
		
		if (buffer[14] > '9') {
			param2 = buffer[14] - '7';
		} else {
			param2 = buffer[14] - '0';
		}
		
		lenIPD = (param1 << 4) + param2;
	}

	if ((bufferIndex == (lenIPD + 18)) && (data == 0x03)){
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
		lenIPD = 0;
	} else if (bufferIndex > (lenIPD + 18)) {
		isIPD = 0;
		bufferIndex = 0;
		lenIPD = 0;	
	}
}

static inline void __gmsReceiveData(unsigned char data, GsmTaskMessageType type) {
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
		bufferIndex = 0;
	} else if (data != 0x0D) {
		buffer[bufferIndex++] = data;
	}
}


void USART3_IRQHandler(void) {
	unsigned char data;
	if (USART_GetITStatus(SerX, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(SerX);
#if defined (__MODEL_DEBUG__)	
	USART_SendData(UART5, data);
#endif	
	USART_ClearITPendingBit(SerX, USART_IT_RXNE);
	if (isIPD) {
		__gmsReceiveIPDData(data);		
		return;
	}

	if (isRTC) {
		__gmsReceiveData(data, TYPE_RTC_DATA);
		isRTC = 0;
		return;
	}
	
	if(isCSQ) {
		__gmsReceiveData(data, TYPE_CSQ_DATA);
		isCSQ = 0;
		return;
	}
	
	if(isCops) {
		__gmsReceiveData(data, TYPE_COPS_DATA);
		isCops = 0;
		return;
	}
	
	if (data == 0x0A) {
		buffer[bufferIndex++] = 0;
		if (bufferIndex >= 2) {
			portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
			const GSMAutoReportMap *p;
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
		if(data == 0x02) {
			bufferIndex = 0;
			buffer[bufferIndex++] = data;
			isIPD = 1;
		}
		if (strncmp(buffer, "*PSUTTZ:", 8) == 0) {
			bufferIndex = 0;
			isRTC = 1;
		}
		
		if (strncmp(buffer, "+CSQ: ", 6) == 0) {
			bufferIndex = 0;
			isCSQ = 1;
		}
		
		if (strncmp(buffer, "CLOSED", 6) == 0) {
			bufferIndex = 0;
			TcpConnect = 0;
		}
		
		if (strncmp(buffer, "+CME ERROR: 3", 13) == 0){
			bufferIndex = 0;
			SENDERROE = 1;
		}
		
		if (strncmp(buffer, "+COPS: 0,0,", 11) == 0) {
			bufferIndex = 0;
			isCops = 1;
		}
	}
	
}

/// Start GSM modem.
void __gsmModemStart(){
	GPIO_SetBits(GPIO_GSM, Pin_Restart);
	vTaskDelay(configTICK_RATE_HZ);
	GPIO_ResetBits(GPIO_GSM, Pin_Restart);
	vTaskDelay(configTICK_RATE_HZ);

	GPIO_ResetBits(GPIO_GSM, Pin_Reset);
	vTaskDelay(configTICK_RATE_HZ);
	GPIO_SetBits(GPIO_GSM, Pin_Reset);
	vTaskDelay(configTICK_RATE_HZ * 3);		
}

/// Check if has the GSM modem connect to a TCP server.
/// \return true   When the GSM modem has connect to a TCP server.
/// \return false  When the GSM modem dose not connect to a TCP server.


bool __gsmIsTcpConnected() {
	char *reply;
	while (1) {
		reply = ATCommand("AT+CIPSTATUS\r", "STATE:", configTICK_RATE_HZ * 2);
		if (reply == NULL) {
			return false;
		}
		if (strncmp(&reply[7], "CONNECT OK", 10) == 0) {
			AtCommandDropReplyLine(reply);
			return true;
		}
		if (strncmp(&reply[7], "TCP CONNECTING", 14) == 0) {
			AtCommandDropReplyLine(reply);
			vTaskDelay(configTICK_RATE_HZ * 5);
			continue;
		}
		if (strncmp(&reply[7], "TCP CLOSED", 10) == 0) {
			AtCommandDropReplyLine(reply);
			return false;
		}
		AtCommandDropReplyLine(reply);
		break;
	}
	return false;
}

static char array = 0;
static char Cache[5][200] = {0};

bool GSMTaskSendErrorTcpData(void) {
	int i, j, len;
	char buf[18];
	char *reply, *p;
	
	array = 0;
	for(i = 0; i < 5; i++){
		len = Cache[i][0];
		if(len == 0){
			continue;
		}
		sprintf(buf, "AT+CIPSEND=%d\r", len);		  //len多大1460
		
		ATCommand(buf, NULL, configTICK_RATE_HZ / 4);
		
		p = &(Cache[i][1]);
		for (j = 0; j < len; j++) {
			ATCmdSendChar(*p++);
		}
		
		reply = ATCommand(NULL, "DATA", configTICK_RATE_HZ / 5);
		if (reply == NULL) {
			return false;
		}

		if (0 == strncmp(reply, "DATA ACCEPT", 11)) {
			memset(Cache[i], 0, 200);
			AtCommandDropReplyLine(reply);
			continue;
		} else {
			AtCommandDropReplyLine(reply);
		}
	}
	return true;
}


bool __gsmSendTcpDataLowLevel(const char *p, int len) {
	int i;
	char buf[18];
	char *reply;
	char *ret;
	
	ret = (char *)p;
  sprintf(buf, "AT+CIPSEND=%d\r", len);		  //len多大1460
	
	while (1) {	
		ATCommand(buf, NULL, configTICK_RATE_HZ / 20);
		for (i = 0; i < len; i++) {
			ATCmdSendChar(*p++);
		}
		reply = ATCommand(NULL, "DATA", configTICK_RATE_HZ / 10);
		if (reply == NULL) {
			if(array > 5){
				array = 0;
			}
			if(len >= 18){
				Cache[array][0] = len;
				strcpy(&(Cache[array++][1]), ret);
			}
			return false;
		}

		if (0 == strncmp(reply, "DATA ACCEPT", 11)) {
			AtCommandDropReplyLine(reply);
			return true;
		}else {
			AtCommandDropReplyLine(reply);
		}
	}
}

char TCPStatus(char type, char value){
	if(type == 0){
		return TcpConnect;
	} else {
		TcpConnect = value;
		return TcpConnect;
	}
}
bool __gsmCheckTcpAndConnect(const char *ip, unsigned short port) {
	char buf[44];
	char *reply;
	if (__gsmIsTcpConnected()) {
		if(TcpConnect == 0){
			TcpConnect = 1;
		}
		return true;
	} else {
		TcpConnect = 0;
	}

  if (!ATCommandAndCheckReply("AT+CIPSHUT\r", "SHUT", configTICK_RATE_HZ * 3)) {
		printf("AT+CIPSHUT error\r");
		return false;
	}

	sprintf(buf, "AT+CIPSTART=\"TCP\",\"%s\",%d\r", ip, port);    /*设备出厂前要先设置好IP和端口及网关地址*/
	reply = ATCommand(buf, "CONNECT", configTICK_RATE_HZ * 5);  
	if (reply == NULL) {
		return false;
	}

	if (strncmp("CONNECT OK", reply, 10) == 0) {
		AtCommandDropReplyLine(reply);
		//NorFlashWriteChar(NORFLASH_MANAGEM_ADDR, (const char *)&__gsmRuntimeParameter, sizeof(GMSParameter));
		return true;
	}
	AtCommandDropReplyLine(reply);
	return false;
}

bool __initGsmRuntime() {
	int i;
	static const int bauds[] = {57600, 115200, 38400, 19200};
	__gsmModemStart();
	for (i = 0; i < ARRAY_MEMBER_NUMBER(bauds); ++i) {
		// 设置波特率
		printf("Init gsm baud: %d\n", bauds[i]);
		__gsmInitUsart(bauds[i]);
		ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ * 2);
		ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ * 2);

		if (ATCommandAndCheckReply("ATE0\r", "OK", configTICK_RATE_HZ * 2)) {	  //回显模式关闭
			break;
		}
	}
	if (i >= ARRAY_MEMBER_NUMBER(bauds)) {
		printf("All baud error\n");
		return false;
	}
	
	vTaskDelay(configTICK_RATE_HZ * 2);

	if (!ATCommandAndCheckReply("AT+IPR=57600\r", "OK", configTICK_RATE_HZ * 2)) {		   //设置通讯波特率
		printf("AT+IPR=115200 error\r");
		return false;
	}
	
  if (!ATCommandAndCheckReply("AT+CMEE=1\r", "OK", configTICK_RATE_HZ * 5)) {		 //上报移动设备错误
		printf("AT+CMEE error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CLTS=1\r", "OK", configTICK_RATE_HZ)) {		   //获取本地时间戳
		printf("AT+CLTS error\r");
		return false;
	}

	if (!ATCommandAndCheckReply("AT+CIPHEAD=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+CIPHEAD error\r");
		return false;
	}		   //配置接受数据时是否显示IP头

	if (!ATCommandAndCheckReply("AT+CIPSRIP=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+CIPSRIP error\r");
		return false;
	}		  //配置接受数据时是否显示发送方的IP地址和端口号

	if (!ATCommandAndCheckReply("AT+CIPSHOWTP=1\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+CIPSHOWTP error\r");
		return false;
	}		   //配置接受数据IP头是否显示传输协议

	if (!ATCommandAndCheckReply("AT+CIPCSGP=1,\"CMNET\"\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+CIPCSGP error\r");
  		return false;
	}			//打开GPRS连接

	if (!ATCommandAndCheckReply("AT+CIPQSEND=1\r", "OK", configTICK_RATE_HZ / 5)) {  //选择快发模式
		printf("AT+CIPQSEND error\r");
		return false;
	}	
	
	return true;
}

const char *GetBack(void){
	const char *data = BROACAST;
	return data;
}

void __handleSMS(GsmTaskMessage *p) {
	SMSInfo *sms;
	const char *dat = __gsmGetMessageData(p);
	sms = __gsmPortMalloc(sizeof(SMSInfo));
	printf("Gsm: got sms => %s\n", dat);
	SMSDecodePdu(dat, sms);
	if(sms->contentLen == 0)  return;

//	ProtocolHandlerSMS(sms);
	__gsmPortFree(sms);
}

void __handleProtocol(GsmTaskMessage *msg) {
	GMSParameter g;
	ProtocolHead *h = __gsmPortMalloc(sizeof(ProtocolHead));
	
	h->header = 0x02;
	sscanf((const char *)&msg[1], "%*1s%10s", h->addr);
	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&g, (sizeof(GMSParameter) + 1) / 2);
	if((strncmp((char *)&(h->addr), (char *)GetBack(), 10) != 0) && (strncmp((char *)&(h->addr), (char *)&(g.GWAddr), 10) != 0)){
		return;
	}
	
	memcpy(&(h->addr), (const char *)&(g.GWAddr), 10);

	sscanf((const char *)&msg[1], "%*11s%2s", h->contr);
	sscanf((const char *)&msg[1], "%*13s%2s", h->lenth);

	ProtocolHandler(h, __gsmGetMessageData(msg));

	__gsmPortFree(h);
}

void __handleSendTcpDataLowLevel(GsmTaskMessage *msg) {
#if defined (__MODEL_DEBUG__)	
	 printf("%s.\r\n", __gsmGetMessageData(msg));
#endif	
	 __gsmSendTcpDataLowLevel(__gsmGetMessageData(msg), msg->length);
}

void trans(char *tmpa, char tmpb, char *tmpd){
	if((tmpd[tmpb-3] - '0') > 0){
		*tmpa = (tmpd[tmpb - 3] - '0') * 10 + (tmpd[tmpb - 2] - '0');
	} else {
		*tmpa = (tmpd[tmpb - 2] - '0');
	}
}

void __handleM35RTC(GsmTaskMessage *msg) {
	DateTime dateTime;
	unsigned short i, j=0;
	char *p = __gsmGetMessageData(msg), tmp[8];	 
	static const uint16_t Common_Year[] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
	
	static const uint16_t Leap_Year[] = {
	0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};
	
	sscanf(p, "%[^,]", tmp);
	for(i = 0; i < 8; i++){
		if(tmp[i] == 0x20){
			tmp[i] = '0';
		}
	}
	i = atoi((const char *)tmp);

	dateTime.year = i - 2000;
	for(i=4; i<100; i++){
		if(p[i] == 0x0D){
			break;
		}
		
		if(p[i] == 0x20){
			j++;
		} else {
			continue;
		}
		
		if(j == 2){
			trans((char*)&(dateTime.month), i, p);
		} else if (j == 3){
			trans((char*)&(dateTime.date), i, p);
		} else if (j == 4){
			trans((char*)&(dateTime.hour), i, p);
			dateTime.hour= dateTime.hour + 8;
		} else if (j == 5){
			trans((char*)&(dateTime.minute), i, p);
		} else if (j == 6){
			trans((char*)&(dateTime.second), i, p);
		} 
	}
	
	i = dateTime.date;
	if(dateTime.hour >= 24) {
		i += 1;
		dateTime.hour = dateTime.hour - 24;
		dateTime.date += 1;
		if( 0 == dateTime.year / 4){
			i += Leap_Year[dateTime.month - 1];
			if(i > Leap_Year[dateTime.month]){
				dateTime.date = 1;
				dateTime.month += 1;
				if (dateTime.month > 12) {
					dateTime.month = 1;
					dateTime.year += 1;		
				}
			}
		} else {
			i += Common_Year[dateTime.month - 1];
			if(i > Common_Year[dateTime.month]){
				dateTime.date = 1;
				dateTime.month += 1;
				if (dateTime.month > 12) {
					dateTime.month = 1;
					dateTime.year += 1;		
				}
			}
		}
	}
  
	RtcSetTime(DateTimeToSecond(&dateTime));
}

void __handleSendAtCommand(GsmTaskMessage *msg) {
	ATCommand(__gsmGetMessageData(msg), NULL, configTICK_RATE_HZ / 10);
}

static unsigned char Vcsq = 0;
static unsigned char Vber = 0;

unsigned char RSSIValue(unsigned char p){
	if(p == 1){
		return Vcsq;
	} else {
		return Vber;
	}
}

void __handleCSQ(GsmTaskMessage *msg) {
	char *dat = __gsmGetMessageData(msg);
	char buf[3];
	if (dat[0] == 0x20) {
		dat++;
	}

	if ((dat[0] > 0x33) && (dat[0] < 0x30)){
		return;
	}
	
	Vcsq = (dat[0] - '0') * 10 + dat[1] - '0';
	
	sscanf(dat, "%*[^,]%*c%s", buf);
	Vber = atoi(buf);	
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

void __handleCOPS(GsmTaskMessage *msg) {
	char *dat = __gsmGetMessageData(msg);
	*dat++;
	if(strncasecmp(dat, "CHINA MOBILE", 12) == 0){
		CMCC = 1;
	} else if(strncasecmp(dat, "CHINA UNICOM", 12) == 0){
		CMCC = 2;
	}
}


typedef struct {
	GsmTaskMessageType type;
	void (*handlerFunc)(GsmTaskMessage *);
} MessageHandlerMap;

static const MessageHandlerMap __messageHandlerMaps[] = {
	{ TYPE_SMS_DATA, __handleSMS },
	{ TYPE_GPRS_DATA, __handleProtocol },
	{ TYPE_SEND_TCP_DATA, __handleSendTcpDataLowLevel },
	{ TYPE_RTC_DATA, __handleM35RTC},
	{ TYPE_SEND_AT, __handleSendAtCommand },
	{ TYPE_COPS_DATA, __handleCOPS},
	{ TYPE_CSQ_DATA, __handleCSQ },
	{ TYPE_SEND_SMS, __handleSendSMS },
	{ TYPE_NONE, NULL },
};

bool __GPRSmodleReset(void){
	__gsmModemStart();
	
	if(__initGsmRuntime()){
		return true;
	}
	return false;
}

static void __gsmTask(void *parameter) {
	portBASE_TYPE rc;
	GsmTaskMessage *message;
	portTickType lastT = 0;
	portTickType curT;	
	
	while (1) {
		printf("Gsm start\n");
		if (__initGsmRuntime()) {
			break;
		}		   
	}

	for (;;) {
//		printf("Gsm: loop again\n");					
		curT = xTaskGetTickCount();
		rc = xQueueReceive(__queue, &message, configTICK_RATE_HZ );
		if (rc == pdTRUE) {
			const MessageHandlerMap *map = __messageHandlerMaps;
			for (; map->type != TYPE_NONE; ++map) {
				if (message->type == map->type) {
					map->handlerFunc(message);
					break;
				}
			}
			__gsmDestroyMessage(message);
			message = NULL;
		} else if((curT - lastT) > configTICK_RATE_HZ * 3){
			
//			NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&__gsmRuntimeParameter, (sizeof(GMSParameter)  + 1)/ 2);
//			
//			if(__gsmRuntimeParameter.GWAddr[0] == 0xFF){
//				continue;
//			}
			
			if (0 == __gsmCheckTcpAndConnect(__gsmRuntimeParameter.serverIP, __gsmRuntimeParameter.serverPORT)) {
				printf("Gsm: Connect TCP error\n");
			} 
			lastT = curT;
		}
	}
}

void GSMInit(void) {
	ATCommandRuntimeInit();
	__gsmInitHardware();
	__queue = xQueueCreate(20, sizeof( GsmTaskMessage*));
	xTaskCreate(__gsmTask, (signed portCHAR *) "GSM", GSM_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
}
