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
#include "zklib.h"
#include "atcmd.h"
#include "norflash.h"
#include "second_datetime.h"

#define BROACAST   "9999999999"

#define GSM_TASK_STACK_SIZE			     (configMINIMAL_STACK_SIZE + 1024 * 3)

#define __gsmPortMalloc(size)        pvPortMalloc(size)
#define __gsmPortFree(p)             vPortFree(p)

#define  SerX               USART3

#define  GPIO_GSM           GPIOB
#define  GSM_Tx             GPIO_Pin_10
#define  GSM_Rx             GPIO_Pin_11

#define  GPIO_Reset         GPIOC
#define  Pin_Reset          GPIO_Pin_0

#define  GPIO_GPRS_PW_EN    GPIOB
#define  PIN_GPRS_PW_EN     GPIO_Pin_7

static xQueueHandle __queue;

static GMSParameter __gsmRuntimeParameter;

void ATCmdSendChar(char c) {
	USART_SendData(SerX, c);
	while (USART_GetFlagStatus(SerX, USART_FLAG_TXE) == RESET);
}

static void ATCmdSendStr(char *s){
	while(*s){
		ATCmdSendChar(*s++);
	}
}

static void GPRSDataTransparent(char *str, int len){
	int i;
	
	for(i = 0; i < len; i++){
		ATCmdSendChar(*str++);
	}	
}

typedef enum {
	TYPE_NONE = 0,
	TYPE_SMS_DATA,
	TYPE_GPRS_DATA,
	TYPE_RTC_DATA,
	TYPE_SEND_TCP_DATA,
	TYPE_CSQ_DATA,
	TYPE_SEND_AT,
	TYPE_SEND_SMS,
} GsmTaskMessageType;

typedef struct {
	GsmTaskMessageType type;
	unsigned char  length;
	char infor[240];
} GsmTaskMessage;

static inline void *__gsmGetMessageData(GsmTaskMessage *message) {
	return message->infor;
}

void __gsmDestroyMessage(GsmTaskMessage *message) {
	__gsmPortFree(message);
}

bool GsmTaskSendAtCommand(const char *atcmd) {
	int len = strlen(atcmd);
	GsmTaskMessage message;
	
	message.type = TYPE_SEND_AT;
	message.length = len;
	memcpy(message.infor, atcmd, len);
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 5)) {
		return false;
	}
	return true;
}

bool GsmTaskSendTcpData(const char *dat, unsigned char len) {
	GsmTaskMessage message;
	
	message.type = TYPE_SEND_TCP_DATA;
	message.length = len;
	memcpy(message.infor, dat, len);
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 5)) {
		return true;
	}
	return false;
}

bool GsmTaskSendSMS(const char *pdu, int len) {
  GsmTaskMessage message;
	
	message.type = TYPE_SEND_SMS;
	message.length = len;
	memcpy(message.infor, pdu, len);
	
	if (pdTRUE != xQueueSend(__queue, &message, configTICK_RATE_HZ * 5)) {
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

	GPIO_InitStructure.GPIO_Pin =  GSM_Tx;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_GSM, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GSM_Rx;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIO_GSM, &GPIO_InitStructure);				   //GSM模块的串口

  GPIO_SetBits(GPIO_Reset, Pin_Reset);

  GPIO_InitStructure.GPIO_Pin = Pin_Reset;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_Reset, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin =  PIN_GPRS_PW_EN;     //使能GSM模块电源脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_GPRS_PW_EN, &GPIO_InitStructure);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
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
	{ "0891", TYPE_SMS_DATA },
	{ NULL, TYPE_NONE },
};


static char buffer[240];
static char bufferIndex = 0;
static char isIPD = 0;
static char isRTC = 0;
static unsigned char lenIPD = 0;
static char TCPLost = 0; 
static char isCSQ = 0;

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
		GsmTaskMessage message;
		buffer[bufferIndex++] = 0;

		message.type = TYPE_GPRS_DATA;
		message.length = bufferIndex;
		memcpy(message.infor, buffer, bufferIndex);
		
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
		GsmTaskMessage message;
		portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
		buffer[bufferIndex++] = 0;
		
		message.type = type;
		message.length = bufferIndex;
		memcpy(message.infor, buffer, bufferIndex);
		
		if (pdTRUE == xQueueSendFromISR(__queue, &message, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				taskYIELD();
			}
		}
		bufferIndex = 0;
		if(type == TYPE_RTC_DATA)
			isRTC = 0;
		else if(type == TYPE_CSQ_DATA)
			isCSQ = 0;
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
		return;
	}
	
	if(isCSQ) {
		__gmsReceiveData(data, TYPE_CSQ_DATA);
		return;
	}
	
	if ((data == 0x0A)  || (bufferIndex == 238)) {
		buffer[bufferIndex++] = 0;
		
		if (bufferIndex >= 2) {
			portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
			const GSMAutoReportMap *p;
			for (p = __gsmAutoReportMaps; p->prefix != NULL; ++p) {
				if (strncmp(p->prefix, buffer, strlen(p->prefix)) == 0) {
					GsmTaskMessage message;
					
					message.type = p->type;
					message.length = bufferIndex;
					memcpy(message.infor, buffer, bufferIndex);
					
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
		
		if ((strncmp(buffer, "CLOSED", 6) == 0) || (strncmp(buffer, "CONNECT FAIL", 12) == 0)) {
			bufferIndex = 0;
			TCPLost = 1;
		}
	}
	
}

void __gsmModemStart(){
//	GPIO_SetBits(GPIO_GSM, Pin_Restart);
//	vTaskDelay(configTICK_RATE_HZ / 2);
//	GPIO_ResetBits(GPIO_GSM, Pin_Restart);
//	vTaskDelay(configTICK_RATE_HZ / 2);

	GPIO_ResetBits(GPIO_Reset, Pin_Reset);
	vTaskDelay(configTICK_RATE_HZ / 2);
	GPIO_SetBits(GPIO_Reset, Pin_Reset);
	vTaskDelay(configTICK_RATE_HZ * 3);		
}

static void RemainTCPConnect(void){
	
	ATCmdSendStr("DM");
}

static void RelinkTCP(void){
	char buf[48];
	
	if (!ATCommandAndCheckReply("AT+CGATT?\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+CGATT error\r");
	}	
	
	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&__gsmRuntimeParameter, (sizeof(GMSParameter)  + 1)/ 2);
	sprintf(buf, "AT+CIPSTART=\"TCP\",\"%s\",%d\r", __gsmRuntimeParameter.serverIP, __gsmRuntimeParameter.serverPORT);
	
	if (!ATCommandAndCheckReply(buf, "CONNECT", configTICK_RATE_HZ * 5)) {
		printf("AT+CIPSTART error\r");
		return;
	}
	
	TCPLost = 0;
}

bool __gsmSendTcpDataLowLevel(const char *p, int len) {
	GPRSDataTransparent((char *)p, len);
}

bool __initGsmRuntime() {
	int i;
	static const int bauds[] = {57600, 19200};
	char buf[48];

	__gsmModemStart();
	for (i = 0; i < ARRAY_MEMBER_NUMBER(bauds); ++i) {
		// 设置波特率
		printf("Init gsm baud: %d\n", bauds[i]);
		__gsmInitUsart(bauds[i]);
		ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ / 2);
		ATCommandAndCheckReply("AT\r", "OK", configTICK_RATE_HZ / 2);

		if (ATCommandAndCheckReply("ATE0\r", "OK", configTICK_RATE_HZ / 2)) {	  //回显模式关闭
			break;
		}
	}
	if (i >= ARRAY_MEMBER_NUMBER(bauds)) {
		printf("All baud error\n");
		return false;
	}
	
	vTaskDelay(configTICK_RATE_HZ * 2);

	if (!ATCommandAndCheckReply("AT+IPR=57600\r", "OK", configTICK_RATE_HZ / 2)) {		   //设置通讯波特率
		printf("AT+IPR error\r");
		return false;
	}	
	
	if (!ATCommandAndCheckReply(NULL, "Call Ready", configTICK_RATE_HZ * 20)) {
			printf("Wait Call Realy timeout\n");
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

	if (!ATCommandAndCheckReply("AT+CLTS=1\r", "OK", configTICK_RATE_HZ / 2)) {		   //获取本地时间戳
		printf("AT+CLTS error\r");
		return false;
	}
	
	if (!ATCommandAndCheckReply("AT+CIPMODE=1\r", "OK", configTICK_RATE_HZ / 2)) {
		printf("AT+CIPMODE error\r");
		return false;
	}	
	
	if (!ATCommandAndCheckReply("AT+CGDCONT=1,\"IP\",\"CMNET\"\r", "OK", configTICK_RATE_HZ / 2)) {
		printf("AT+CGDCONT error\r");
		return false;
	}	
	
	if (!ATCommandAndCheckReply("AT+CIPCCFG=5,3,1024,1\r", "OK", configTICK_RATE_HZ / 2)) {
		printf("AT+CIPCCFG error\r");
		return false;
	}	

	if (!ATCommandAndCheckReply("AT+CGATT=0\r", "OK", configTICK_RATE_HZ * 2)) {
			printf("AT+CGATT=0 error\r");
			return false;
  }
	
	if (!ATCommandAndCheckReply("AT+CGATT=1\r", "OK", configTICK_RATE_HZ * 15)) {
		printf("AT+CGATT=1 error\r");
		return false;
	}	
	
	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&__gsmRuntimeParameter, (sizeof(GMSParameter)  + 1)/ 2);	
	
	sprintf(buf, "AT+CIPSTART=\"TCP\",\"%s\",%d\r", __gsmRuntimeParameter.serverIP, __gsmRuntimeParameter.serverPORT);
	
	if (!ATCommandAndCheckReply(buf, "CONNECT", configTICK_RATE_HZ * 10)) {
		printf("AT+CIPSTART error\r");
		return false;
	}
	
	return true;
}

void SwitchCommand(void){
	vTaskDelay(configTICK_RATE_HZ);
	ATCmdSendStr("+++");
	vTaskDelay(configTICK_RATE_HZ / 2);
}

void SwitchData(void){
	if (!ATCommandAndCheckReply("ATO\r", "CONNECT", configTICK_RATE_HZ )) {
		printf("ATO error\r");
	}	
}

const char *GetBack(void){
	const char *data = BROACAST;
	return data;
}

extern void ProtocolHandlerSMS(const SMSInfo *sms);

void __handleSMS(GsmTaskMessage *p) {
	SMSInfo *sms;
	const char *dat = __gsmGetMessageData(p);
	
	sms = __gsmPortMalloc(sizeof(SMSInfo));
//	printf("Gsm: got sms => %s\n", dat);
  SMSDecodePdu(dat, sms);
	if(sms->contentLen == 0)  return;

	SwitchData();
	ProtocolHandlerSMS(sms);
	
	__gsmPortFree(sms);
}

void __handleProtocol(GsmTaskMessage *msg) {
	GMSParameter g;
	const char *dat = __gsmGetMessageData(msg);
	ProtocolHead *h = __gsmPortMalloc(sizeof(ProtocolHead));
	
	h->header = 0x02;
	sscanf((const char *)dat, "%*1s%10s", h->addr);
	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&g, (sizeof(GMSParameter) + 1) / 2);
	if((strncmp((char *)&(h->addr), (char *)GetBack(), 10) != 0) && (strncmp((char *)&(h->addr), (char *)&(g.GWAddr), 10) != 0)){
		return;
	}
	
	memcpy(&(h->addr), (const char *)&(g.GWAddr), 10);

	sscanf((const char *)dat, "%*11s%2s", h->contr);
	sscanf((const char *)dat, "%*13s%2s", h->lenth);

	GPRSProtocolHandler(h, (char *)dat);

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

void __handleSIM900RTC(GsmTaskMessage *msg) {
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

extern unsigned char *ProtocolRespond(unsigned char address[10], unsigned char  type[2], const char *msg, unsigned char *size);

void __handleCSQ(GsmTaskMessage *msg) {
	char *dat = __gsmGetMessageData(msg);
	unsigned char buf[5], *tmp, i, size;
	if (dat[0] == 0x20) {
		dat++;
	}

	if ((dat[0] > 0x33) && (dat[0] < 0x30)){
		return;
	}
	
	Vcsq = (dat[0] - '0') * 10 + dat[1] - '0';
	
	sscanf(dat, "%*[^,]%*c%s", buf);
	Vber = atoi(buf);	
	
	sprintf((char *)buf, "%2d%2d", Vcsq, Vber);	
	for(i = 0; i < 4; i++){
		if(buf[i] == 0x20){
			buf[i] = '0';
		}
	}
	SwitchData();
	
	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&__gsmRuntimeParameter, (sizeof(GMSParameter)  + 1)/ 2);
	
	tmp = ProtocolRespond(__gsmRuntimeParameter.GWAddr, "17", (const char *)buf, &size);
  GsmTaskSendTcpData((const char *)tmp, size);
	vPortFree((void*)tmp);	
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
	
//	SwitchData();
}


typedef struct {
	GsmTaskMessageType type;
	void (*handlerFunc)(GsmTaskMessage *);
} MessageHandlerMap;

static const MessageHandlerMap __messageHandlerMaps[] = {
	{ TYPE_SMS_DATA, __handleSMS },
	{ TYPE_GPRS_DATA, __handleProtocol },
	{ TYPE_SEND_TCP_DATA, __handleSendTcpDataLowLevel },
	{ TYPE_RTC_DATA, __handleSIM900RTC},
	{ TYPE_SEND_AT, __handleSendAtCommand },
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
	GsmTaskMessage message;
	portTickType lastT = 0, heartT = 0;
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
		rc = xQueueReceive(__queue, &message, configTICK_RATE_HZ / 10);
		if (rc == pdTRUE) {
			const MessageHandlerMap *map = __messageHandlerMaps;
			for (; map->type != TYPE_NONE; ++map) {
				if (message.type == map->type) {
					map->handlerFunc(&message);
					break;
				}
			}
			
		} else if((TCPLost) && ((curT - lastT) > configTICK_RATE_HZ / 5)){
			RelinkTCP();
			lastT = curT;
		} else if((curT - heartT) > configTICK_RATE_HZ * 60){
			RemainTCPConnect();
			heartT = curT;
		}
//		else if((curT - lastT) > configTICK_RATE_HZ * 3){
//			
//			NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&__gsmRuntimeParameter, (sizeof(GMSParameter)  + 1)/ 2);
//			
//			if(__gsmRuntimeParameter.GWAddr[0] == 0xFF){
//				continue;
//			}
//			
//			if (0 == __gsmCheckTcpAndConnect(__gsmRuntimeParameter.serverIP, __gsmRuntimeParameter.serverPORT)) {
//				printf("Gsm: Connect TCP error\n");
//			} 
//			lastT = curT;
//		}
	}
}

void GSMInit(void) {
	ATCommandRuntimeInit();
	__gsmInitHardware();
	__queue = xQueueCreate(10, sizeof( GsmTaskMessage));
	xTaskCreate(__gsmTask, (signed portCHAR *) "GSM", GSM_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
}
