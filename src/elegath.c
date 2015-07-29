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
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "misc.h"
#include "elegath.h"
#include "protocol.h"
#include "zklib.h"
#include "norflash.h"
#include "second_datetime.h"


#define ELECTRIC_TASK_STACK_SIZE		(configMINIMAL_STACK_SIZE + 1024)

static xQueueHandle __ElectQueue;

#define BAUD   9600

#define PIN_ELEC_TX   GPIO_Pin_2   //USART1  GPIO_Pin_9     //USART2  GPIO_Pin_2
#define PIN_ELEC_RX   GPIO_Pin_3 //USART1  GPIO_Pin_10    //USART2  GPIO_Pin_3
#define GPIO_ELEC     GPIOA

#define COM_ELEC      USART2
#define COMy_IRQ      USART2_IRQn

static inline void __ElectrolHardwareInit(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef   USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	GPIO_InitStructure.GPIO_Pin =  PIN_ELEC_TX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_ELEC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_ELEC_RX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIO_ELEC, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = BAUD;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(COM_ELEC, &USART_InitStructure);
	USART_ITConfig(COM_ELEC, USART_IT_RXNE, ENABLE);
	USART_Cmd(COM_ELEC, ENABLE);
	
//	USART_ClearFlag(COM_PRINT, USART_FLAG_TC); 
  USART_ClearFlag(COM_ELEC, USART_FLAG_TXE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

	NVIC_InitStructure.NVIC_IRQChannel = COMy_IRQ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

typedef enum{
	TYPE_RECIEVE_DATA,
	TYPE_SEND_DATA,
	TYPE_NONE,
}ElecTaskMessageType;

typedef struct {
	/// Message type.
	ElecTaskMessageType type;
	/// Message lenght.
	unsigned char length;
} ElecTaskMsg;

//static void *ElectaskApplyMemory(int size){
////	printf("M");
//	return pvPortMalloc(size);
//}

//static void ElecTaskFreeMemory(void *p){
////	printf("F");
//	vPortFree(p);
//}

static ElecTaskMsg *__ElectCreateMessage(ElecTaskMessageType type, const char *dat, unsigned char len) {
  ElecTaskMsg *message = pvPortMalloc(ALIGNED_SIZEOF(ElecTaskMsg) + len);
	if (message != NULL) {
		message->type = type;
		message->length = len;
		memcpy(&message[1], dat, len);
	}
	return message;
}

static inline void *__ElecGetMsgData(ElecTaskMsg *message) {
	return &message[1];
}

static void EleComSendChar(char c) {
	USART_SendData(COM_ELEC, c);
	while (USART_GetFlagStatus(COM_ELEC, USART_FLAG_TXE) == RESET);
}

static char buffer[255];
static char bufferIndex = 0;
static char isPRO = 0;
static unsigned char LenPRO = 0;

void USART2_IRQHandler(void) {
	unsigned char data;
	char param1, param2;
	
	if (USART_GetITStatus(COM_ELEC, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(COM_ELEC);
#if defined (__MODEL_DEBUG__)	
	USART_SendData(UART5, data);
#endif	
	USART_ClearITPendingBit(COM_ELEC, USART_IT_RXNE);
	
	if(isPRO){
		if((data >= '0') && (data <= 'F')) {
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
				
				LenPRO = (param1 << 4) + param2;
			}
		} else if(data == 0x03){
			buffer[bufferIndex++] = data;
		} else {
			bufferIndex = 0;
		  isPRO = 0;
			LenPRO = 0;
		}
	}

	if ((bufferIndex == (LenPRO + 18)) && (data == 0x03)) {
		ElecTaskMsg *msg;
		portBASE_TYPE xHigherPriorityTaskWoken;
		buffer[bufferIndex++] = 0;
		msg = __ElectCreateMessage(TYPE_RECIEVE_DATA, (const char *)buffer, bufferIndex);		
		if (pdTRUE == xQueueSendFromISR(__ElectQueue, &msg, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				portYIELD();
			}
		}
		
		bufferIndex = 0;
		isPRO = 0;
		LenPRO = 0;
	} else if (bufferIndex > (LenPRO + 18)) {
		bufferIndex = 0;
		isPRO = 0;
		LenPRO = 0;
	} else if (data == 0x02) {
		bufferIndex = 0;
		buffer[bufferIndex++] = data;
		isPRO = 1;
	}
}

char HexToAscii(char hex){
	if((hex >= 0) && (hex <= 9)){
		hex = hex + '0';
	} else if((hex >= 10) && (hex < 16)){
		hex = hex + '7';
	}
	return hex;
}

static unsigned short Total_Power = 0;

unsigned short PowerStatus(void){
	return Total_Power;
}

extern void *DataFalgQueryAndChange(char Obj, unsigned short Alter, char Query);

static void ElecHandleGWDataQuery(ProtocolHead *header, const char *p){
	Lightparam k;
	GatewayParam1 s;
	GatewayParam3 g;
	unsigned char size;
	char *buf, msg[150], tmp[5], loop = 0;
	int i;
	DateTime dateTime;
	uint32_t second;	
	unsigned short state = 0, Hset, Lset, gath, Cache[3];
	uint16_t Pin_array[] = {PIN_CTRL_1, PIN_CTRL_2, PIN_CTRL_3, PIN_CTRL_4, PIN_CTRL_5, PIN_CTRL_6, PIN_CTRL_7, PIN_CTRL_8};
	GPIO_TypeDef *Gpio_array[] ={GPIO_CTRL_1, GPIO_CTRL_2, GPIO_CTRL_3, GPIO_CTRL_4, GPIO_CTRL_5, GPIO_CTRL_6, GPIO_CTRL_7, GPIO_CTRL_8};

	NorFlashRead(NORFLASH_MANAGEM_WARNING, (short *)&g, (sizeof(GatewayParam3) + 1) / 2);
	
	strcpy(msg, p);
	
	for (i = 0; i < 8; i++){
		if(GPIO_ReadInputDataBit(Gpio_array[i], Pin_array[i]) == 1){
			loop |= (1 << i);
		}
	}
	msg[5] = HexToAscii(loop >> 4);
	msg[6] = HexToAscii(loop & 0x0F);                 /*回路状态*/	
	
	
	NorFlashRead(NORFLASH_MANAGEM_BASE, (short *)&s, (sizeof(GatewayParam1) + 1) / 2);
	sscanf((const char *)s.GatewayID, "%6s", &msg[119]);
	
	NorFlashRead(NORFLASH_LIGHT_NUMBER, (short *)&gath, 1);
	sprintf((char *)&msg[125], "%4d", gath);

	for(i = 0; i < 4; i++){
		if(msg[125 + i] == 0x20){
			msg[125 + i] = '0';
		}
	}
	
	second = RtcGetTime();
	SecondToDateTime(&dateTime, second);
	
	sprintf((char *)&msg[129], "%2d%2d%2d%2d%2d%2d", dateTime.year, dateTime.month, dateTime.date, dateTime.hour, dateTime.minute, dateTime.second);

	for(i = 0; i < 12; i++){
		if(msg[129 + i] == 0x20){
			msg[129 + i] = '0';
		}
	}
	
	msg[strlen(p) - 3] = 0;
	
	if(g.SetWarnFlag != 1){
		
		buf = (char *)ProtocolRespond(header->addr, header->contr, msg, &size);
		GsmTaskSendTcpData((const char *)buf, size);
		vPortFree(buf);
		return;
	}
	
	if(loop == 0){
		sscanf(p, "%*7s%4s", tmp);
		gath = atoi((const char *)tmp);
		if(gath > 10){                                  /*空载时，电压大于10V，认为空载电流过大*/
			state |= (1 << 7);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*7s%4s", tmp);
		gath = atoi((const char *)tmp);
		if(gath > 10){                                  /*空载时，电压大于10V，认为空载电流过大*/
			state |= (1 << 7);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*7s%4s", tmp);
		gath = atoi((const char *)tmp);
		if(gath > 10){                                  /*空载时，电压大于10V，认为空载电流过大*/
			state |= (1 << 7);
			state |= (1 << 15);
		}		
		
		for(i = 0; i < 1000; i++){
			NorFlashRead(NORFLASH_BALLAST_BASE + i * NORFLASH_SECTOR_SIZE, (short *)&k, (sizeof(Lightparam) + 1) / 2);
			if((p[0] == '0') && (k.CommState  == 0x06)){
				state |= (1 << 4);
				state |= (1 << 15);
			} else if((k.Loop == (p[0] - '0')) && (k.CommState  == 0x06)){
				state |= (1 << 4);
				state |= (1 << 15);
			}	
		}
		
		msg[1] = HexToAscii(state >> 12);
		msg[2] = HexToAscii((state >> 8) & 0x0F);
		msg[3] = HexToAscii((state >> 4) & 0x0F);
		msg[4] = HexToAscii(state & 0x0F);                 /*网关运行状态*/	
		
		msg[5] = HexToAscii(loop >> 4);
	  msg[6] = HexToAscii(loop & 0x0F);                 /*回路状态*/

		buf = (char *)ProtocolRespond(header->addr, header->contr, msg, &size);
		GsmTaskSendTcpData((const char *)buf, size);
		vPortFree(buf);
	} else {
	
		sscanf(p, "%*7s%4s", tmp);
		gath = atoi((const char *)tmp);
		sscanf((const char *)g.HVolLimitValL1, "%4s", tmp);
		Hset = atoi((const char *)tmp);
		sscanf((const char *)g.LVolLimitValL1, "%4s", tmp);
		Lset = atoi((const char *)tmp);
		if(gath > Hset){
			state |= (1 << 9);
			state |= (1 << 15);
		} else if(gath < Lset){
			state |= (1 << 8);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*11s%4s", tmp);
		gath = atoi((const char *)tmp);
		sscanf((const char *)g.HVolLimitValL2, "%4s", tmp);
		Hset = atoi((const char *)tmp);
		sscanf((const char *)g.LVolLimitValL2, "%4s", tmp);
		Lset = atoi((const char *)tmp);
		if(gath > Hset){
			state |= (1 << 9);
			state |= (1 << 15);
		} else if(gath < Lset){
			state |= (1 << 8);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*15s%4s", tmp);
		gath = atoi((const char *)tmp);
		sscanf((const char *)g.HVolLimitValL3, "%4s", tmp);
		Hset = atoi((const char *)tmp);
		sscanf((const char *)g.LVolLimitValL3, "%4s", tmp);
		Lset = atoi((const char *)tmp);
		if(gath > Hset){
			state |= (1 << 9);
			state |= (1 << 15);
		} else if(gath < Lset){
			state |= (1 << 8);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*19s%4s", tmp);
		gath = atoi((const char *)tmp);
		sscanf((const char *)g.PhaseCurLimitValL1, "%4s", tmp);
		Hset = atoi((const char *)tmp);
		if(gath > Hset){
			state |= (1 << 6);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*23s%4s", tmp);
		gath = atoi((const char *)tmp);
		sscanf((const char *)g.PhaseCurLimitValL2, "%4s", tmp);
		Hset = atoi((const char *)tmp);
		if(gath > Hset){
			state |= (1 << 6);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*27s%4s", tmp);
		gath = atoi((const char *)tmp);
		sscanf((const char *)g.PhaseCurLimitValL3, "%4s", tmp);
		Hset = atoi((const char *)tmp);
		if(gath > Hset){
			state |= (1 << 6);
			state |= (1 << 15);
		}
		
		sscanf(p, "%*31s%4s", tmp);
		gath = atoi((const char *)tmp);
		sscanf((const char *)g.PhaseCurLimitValN, "%4s", tmp);
		Hset = atoi((const char *)tmp);
		if(gath > Hset){
			state |= (1 << 6);
			state |= (1 << 15);
		}
		
		if(p[0] == '0') {
			sscanf(p, "%*47s%4s", tmp);
			Total_Power = atoi((const char *)tmp);
		}
		
		
		msg[1] = HexToAscii(state >> 12);
		msg[2] = HexToAscii((state >> 8) & 0x0F);
		msg[3] = HexToAscii((state >> 4) & 0x0F);
		msg[4] = HexToAscii(state & 0x0F);                 /*网关运行状态*/	
		
		msg[5] = HexToAscii(loop >> 4);
	  msg[6] = HexToAscii(loop & 0x0F);                 /*回路状态*/

		buf = (char *)ProtocolRespond(header->addr, header->contr, msg, &size);
		GsmTaskSendTcpData((const char *)buf, size);
		vPortFree(buf);
	}	
	
	if(p[0] == '0'){		
		Cache[0] = second >> 16;
		Cache[1] = second & 0xFFFF;
		Cache[2] = 0;
		NorFlashWrite(NORFLASH_ELEC_UPDATA_TIME, (short *)Cache, 2);
	}
	 DataFalgQueryAndChange(5, 0, 0);
}

static void ElecHandleEGVersQuery(ProtocolHead *header, const char *p){
	char *buf, msg[8];
	unsigned char size;
	int i;
	
	memset(msg, 0, 8);
	for(i = 0; i < (strlen(p) - 3); i++){
		msg[i] = p[i];
	}
	
	buf = (char *)ProtocolRespond(header->addr, header->contr, msg, &size);
	GsmTaskSendTcpData((const char *)buf, size);
	
	vPortFree(buf);
}

static void ElecHandleEGUpgrade(ProtocolHead *header, const char *p){
	
}

typedef void (*ElectricHandleFunction)(ProtocolHead *header, const char *p);
typedef struct {
	unsigned char type[2];
	ElectricHandleFunction func;
}ElectricHandleMap;

const static ElectricHandleMap map[] = {  
	{"88",  ElecHandleGWDataQuery},      /*0x08; 网关数据查询*/
	{"8E",  ElecHandleEGVersQuery},      /*0x0E; 查电量采集软件版本号*/
	{"9E",  ElecHandleEGUpgrade},        /*0x1E; 电量采集模块远程升级*/	 
};
	
void __handleRecieve(ElecTaskMsg *p) {
	int i;
	GMSParameter g;
	ProtocolHead h;
	const char *dat = __ElecGetMsgData(p);
	
	h.header = 0x02;
	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&g, (sizeof(GMSParameter) + 1) / 2);
	memcpy(h.addr, (const char *)g.GWAddr, 10);
	
	sscanf((const char *)&dat[1], "%*13s%2s", h.lenth);
	sscanf((const char *)&dat[1], "%*11s%2s", h.contr);
		
	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if(strncmp((const char *)map[i].type, (const char *)h.contr, 2) == 0){
			map[i].func(&h, (dat + 15));
			break;
		}
	}
}

bool ElecTaskSendData(const char *dat, unsigned char len) {
	ElecTaskMsg *message;
  message = __ElectCreateMessage(TYPE_SEND_DATA, dat, len);
	if (pdTRUE != xQueueSend(__ElectQueue, &message, configTICK_RATE_HZ * 5)) {
		vPortFree(message);
		return true;
	}
	return false;
}

void __handleSend(ElecTaskMsg *p){
	int i;
	const char *dat = __ElecGetMsgData(p);
	printf("WG to Elec: %s.\r\n", dat);
	for(i = 0; i < p->length; i++){
		EleComSendChar(*dat++);
	}
}

typedef struct {
	ElecTaskMessageType type;
	void (*handlerFunc)(ElecTaskMsg *);
} MessageHandlerMap;

static const MessageHandlerMap __messageHandlerMaps[] = {
	{ TYPE_RECIEVE_DATA, __handleRecieve},
	{ TYPE_SEND_DATA, __handleSend},
	{ TYPE_NONE, NULL },
};

//void ElectricHandler(ElecTaskMsg *p) {
//	const MessageHandlerMap *map = __messageHandlerMaps;
//	for (; map->type != TYPE_NONE; ++map) {
//		if (p->type == map->type) {
//			map->handlerFunc(p);
//			break;
//		}
//	}
//	
//}

static void EleGathTask(void *parameter) {
	portBASE_TYPE rc;
	ElecTaskMsg *msg;
	
	for (;;) {
		rc = xQueueReceive(__ElectQueue, &msg, portMAX_DELAY);
		if (rc == pdTRUE) {
			//ElectricHandler(msg);
			const MessageHandlerMap *map = __messageHandlerMaps;
			for (; map->type != TYPE_NONE; ++map) {
				if (msg->type == map->type) {
					map->handlerFunc(msg);
					break;
				}
			}		
			vPortFree(msg);
		}
	}
}

void ElectricInit(void) {
	__ElectrolHardwareInit();
	__ElectQueue = xQueueCreate(5, sizeof(ElecTaskMsg *));
	xTaskCreate(EleGathTask, (signed portCHAR *) "ELECTRIC", ELECTRIC_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
}
