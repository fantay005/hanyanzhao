#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "second_datetime.h"
#include "rtc.h"
#include "misc.h"
#include "shuncom.h"
#include "protocol.h"
#include "zklib.h"
#include "norflash.h"
#include "gsm.h"

#define ZIGBEE_TASK_STACK_SIZE			     (configMINIMAL_STACK_SIZE + 1024 * 2)

static xSemaphoreHandle __Zigbeesemaphore;

extern void *DataFalgQueryAndChange(char Obj, char Alter, char Query);

#define COMx      USART1
#define COMx_IRQn USART1_IRQn

#define COM_TX     GPIO_Pin_9
#define COM_RX     GPIO_Pin_10
#define GPIO_COM   GPIOA

static xQueueHandle __ZigbeeQueue;

static char WAITFLAG = 0;

static void SZ05_ADV_Init(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	GPIO_InitStructure.GPIO_Pin =  COM_TX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_COM, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = COM_RX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIO_COM, &GPIO_InitStructure);				   //ZIGBEE模块的串口
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	
	NVIC_InitStructure.NVIC_IRQChannel = COMx_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

static void Zibee_Baud_CFG(int baud){
	USART_InitTypeDef USART_InitStructure;
	
	USART_InitStructure.USART_BaudRate = baud;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(COMx, &USART_InitStructure);
	USART_ITConfig(COMx, USART_IT_RXNE, ENABLE);
	USART_Cmd(COMx, ENABLE);
}

typedef enum{
	TYPE_IOT_RECIEVE_DATA,
	TYPE_IOT_SEND_DATA,
	TYPE_IOT_NONE,
}ZigbTaskMsgType;

typedef struct {
	/// Message type.
	ZigbTaskMsgType type;
	/// Message lenght.
	unsigned char length;
} ZigbTaskMsg;

static void *ZigbtaskApplyMemory(int size){
	return pvPortMalloc(size);
}

static void ZigbTaskFreeMemory(void *p){
	vPortFree(p);
}

static ZigbTaskMsg *__ZigbCreateMessage(ZigbTaskMsgType type, const char *dat, unsigned char len) {
  ZigbTaskMsg *message = ZigbtaskApplyMemory(ALIGNED_SIZEOF(ZigbTaskMsg) + len);
	if (message != NULL) {
		message->type = type;
		message->length = len;
		memcpy(&message[1], dat, len);
	}
	return message;
}

static inline void *__ZigbGetMsgData(ZigbTaskMsg *message) {
	return &message[1];
}

static char buffer[255];
static char bufferIndex = 0;
static char isPROTOC = 0;
static char LenZIGB = 0;

void USART1_IRQHandler(void) {
	unsigned char data;
	char param1, param2;
	
	if (USART_GetITStatus(COMx, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(COMx);
//	USART_SendData(UART5, data);
	USART_ClearITPendingBit(COMx, USART_IT_RXNE);

	//TIM_Cmd(TIMx,DISABLE);
	if(isPROTOC){
		if(((data >= '0') && (data <= 'F')) || (data == 0x03)){
			buffer[bufferIndex++] = data;
			if(bufferIndex == 9) {
				if (buffer[7] > '9') {
					param1 = buffer[7] - '7';
				} else {
					param1 = buffer[7] - '0';
				}
				
				if (buffer[8] > '9') {
					param2 = buffer[8] - '7';
				} else {
					param2 = buffer[8] - '0';
				}
				
				LenZIGB = (param1 << 4) + param2;
			}
		} else {
			bufferIndex = 0;
		  isPROTOC = 0;
			LenZIGB = 0;
		}
	}
	
	if ((bufferIndex == (LenZIGB + 12)) && (data == 0x03)){
		ZigbTaskMsg *msg;
		portBASE_TYPE xHigherPriorityTaskWoken;
		buffer[bufferIndex++] = 0;
		msg = __ZigbCreateMessage(TYPE_IOT_RECIEVE_DATA, (const char *)buffer, bufferIndex);		
		if (pdTRUE == xQueueSendFromISR(__ZigbeeQueue, &msg, &xHigherPriorityTaskWoken)) {
			if (xHigherPriorityTaskWoken) {
				portYIELD();
			}
		}
		bufferIndex = 0;
		isPROTOC = 0;
		LenZIGB = 0;
		WAITFLAG = 1;
	} else if(bufferIndex > (LenZIGB + 12)){
		bufferIndex = 0;
		isPROTOC = 0;
		LenZIGB = 0;
	} else if (data == 0x02){
		bufferIndex = 0;
		buffer[bufferIndex++] = data;
		isPROTOC = 1;
	}
}

extern unsigned char *ProtocolRespond(unsigned char address[10], unsigned char  type[2], const char *msg, unsigned char *size);

SEND_STATUS ZigbTaskSendData(const char *dat, int len) {
	int i, j;
	unsigned short addr;
	unsigned short *mem;
	char hextable[] = "0123456789ABCDEF";
	ZigbTaskMsg *message;
	unsigned char *buf, tmp[5], *ret, size;
	Lightparam k;
	uint16_t Pin_array[] = {PIN_CTRL_1, PIN_CTRL_2, PIN_CTRL_3, PIN_CTRL_4, PIN_CTRL_5, PIN_CTRL_6, PIN_CTRL_7, PIN_CTRL_8};
	GPIO_TypeDef *Gpio_array[] ={GPIO_CTRL_1, GPIO_CTRL_2, GPIO_CTRL_3, GPIO_CTRL_4, GPIO_CTRL_5, GPIO_CTRL_6, GPIO_CTRL_7, GPIO_CTRL_8};
	char build[4] = {'B', '0' , '0' , '0'};
	GMSParameter g;

  ret = DataFalgQueryAndChange(3, 0, 1);

	if(*ret == 1){
	  message = __ZigbCreateMessage(TYPE_IOT_SEND_DATA, dat, len);
		if (pdTRUE != xQueueSend(__ZigbeeQueue, &message, configTICK_RATE_HZ * 5)) {
			ZigbTaskFreeMemory(message);
			return RTOS_ERROR;
		}
		DataFalgQueryAndChange(4, 0, 0);
	//	DataFalgQueryAndChange(3, 0, 0);
		return COM_SUCCESS;
	}
	
	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&g, (sizeof(GMSParameter) + 1) / 2);		
	
//	strcpy((char *)tmp, (const char *)(dat + 3));
//	tmp[4] = 0;
//	addr = atoi((const char *)tmp);
	
#if defined(__HEXADDRESS__)
	addr = (*dat >> 4) * 0x1000 + (*dat & 0x0F) * 0x100 + (*(dat + 1) >> 4) * 0x10 + (*(dat + 1) & 0x0F);
#else
	addr = (*dat >> 4) * 1000 + (*dat & 0x0F) * 100 + (*(dat + 1) >> 4) * 10 + (*(dat + 1) & 0x0F);
#endif
	
	NorFlashRead(NORFLASH_BALLAST_BASE + addr * NORFLASH_SECTOR_SIZE, (short *)&k, (sizeof(Lightparam) + 1) / 2);	
	
	k.UpdataTime = RtcGetTime();
	
	if((k.Loop > '8') || (k.Loop < '0')){
		printf("This loop = %d, have error.\r\n", (k.Loop - '0'));
		return RTOS_ERROR;
	}
	
	mem = DataFalgQueryAndChange(1, 0, 1);
	while(*mem){		
		
		if(*mem == addr){
			if(k.CommState == 0x04){
				if(GPIO_ReadInputDataBit(Gpio_array[0], Pin_array[0]) == 1){
					break;
				}					
				tmp[0] = hextable[k.CommState >> 4];
				tmp[1] = hextable[k.CommState & 0x0F];
				ret = ZigbtaskApplyMemory(38 + 5);
				memset(ret, '0', 43);
				memcpy(ret, build, 4);
				memcpy((ret + 4), (dat + 3), 4);
				memcpy((ret + 4 + 4 + 2), tmp, 2);
				ret[38 + 4] = 0;
				
				buf = ProtocolRespond(g.GWAddr, (unsigned char *)(dat + 7), (const char *)ret, &size);
				GsmTaskSendTcpData((const char *)buf, size);
				ZigbTaskFreeMemory(buf);
				ZigbTaskFreeMemory(ret);
				*mem |= 0x4000;
				return POWER_SHUT;
			}
			break;
		}
		mem++;	
	}

	
  ret = DataFalgQueryAndChange(2, 0, 1);
 
	if(((*ret == 1) || (*ret == 0)) && (!GPIO_ReadOutputDataBit(Gpio_array[k.Loop - '1'], Pin_array[k.Loop - '1']))){
		
    ret = DataFalgQueryAndChange(5, 0, 1);
		if((k.CommState != 0x04) || (*ret == 1)){
			k.CommState = 0x04;
			k.InputPower = 0;
			NorFlashWrite(NORFLASH_BALLAST_BASE + addr * NORFLASH_SECTOR_SIZE, (const short *)&k, (sizeof(Lightparam) + 1) / 2);
			tmp[0] = hextable[k.CommState >> 4];
			tmp[1] = hextable[k.CommState & 0x0F];
			ret = ZigbtaskApplyMemory(38 + 5);
			memset(ret, '0', 43);
			memcpy(ret, build, 4);
			memcpy((ret + 4), (dat + 3), 4);
			memcpy((ret + 4 + 4 + 2), tmp, 2);
			ret[38 + 4] = 0;
			
			buf = ProtocolRespond(g.GWAddr, (unsigned char *)(dat + 7), (const char *)ret, &size);
			GsmTaskSendTcpData((const char *)buf, size);
			ZigbTaskFreeMemory(buf);
			ZigbTaskFreeMemory(ret);
		}
		WAITFLAG = 0;
	  DataFalgQueryAndChange(4, 0, 0);
		return POWER_SHUT;
	}
//	else if (((*ret == 1) || (*ret == 0)) && (GPIO_ReadOutputDataBit(Gpio_array[k.Loop - '1'], Pin_array[k.Loop - '1']))){

//		if(k.CommState == 0x04){
//			k.CommState = 0x03;
//			k.InputPower = 0;
//			NorFlashWrite(NORFLASH_BALLAST_BASE + addr * NORFLASH_SECTOR_SIZE, (const short *)&k, (sizeof(Lightparam) + 1) / 2);
//			tmp[0] = hextable[k.CommState >> 4];
//			tmp[1] = hextable[k.CommState & 0x0F];
//			ret = ZigbtaskApplyMemory(38 + 5);
//			memset(ret, '0', 43);
//			memcpy(ret, build, 4);
//			memcpy((ret + 4), (dat + 3), 4);
//			memcpy((ret + 4 + 4 + 2), tmp, 2);
//			ret[38 + 4] = 0;
//			
//			buf = ProtocolRespond(g.GWAddr, (unsigned char *)(dat + 7), (const char *)ret, &size);
//			GsmTaskSendTcpData((const char *)buf, size);
//			ZigbTaskFreeMemory(buf);
//			ZigbTaskFreeMemory(ret);
//			
//			WAITFLAG = 0;
//	    DataFalgQueryAndChange(4, 0, 0);
//		  return POWER_ENABLE;
//		}	
//	}
	
	message = __ZigbCreateMessage(TYPE_IOT_SEND_DATA, dat, len);
	for(i = 0; i < 3; i++){	
		if (pdTRUE != xQueueSend(__ZigbeeQueue, &message, configTICK_RATE_HZ *5)) {
			ZigbTaskFreeMemory(message);
			return RTOS_ERROR;
		}
//		TIM_Cmd(TIMx, ENABLE); 

		for(j = 0; j < 400; j++){
			if(WAITFLAG == 0){
				vTaskDelay(configTICK_RATE_HZ / 500);
				if(j >= 399){
					WAITFLAG = 2;
				}
			} else {
				break;
			}
		}

//		TIM_Cmd(TIMx, DISABLE); 
		if(WAITFLAG == 1){
			i = 3;
			WAITFLAG = 0;
			if(!GPIO_ReadInputDataBit(Gpio_array[k.Loop - '1'], Pin_array[k.Loop - '1'])){
				if(k.CommState != 0x06){
					k.CommState = 0x06;
					NorFlashWrite(NORFLASH_BALLAST_BASE + addr * NORFLASH_SECTOR_SIZE, (const short *)&k, (sizeof(Lightparam) + 1) / 2);
				}
			}		
			return COM_SUCCESS;
		}
		if((WAITFLAG == 2) && (i != 2)) {
			WAITFLAG = 0;
			continue;
		}
		if((WAITFLAG == 2) && (i == 2)){		
			
			ret = DataFalgQueryAndChange(2, 0, 1);
			if((*ret == 0) && (k.CommState == 0x18) ){			
				WAITFLAG = 0;				
				return STATUS_FIT;
			}		
			
			if(GPIO_ReadInputDataBit(Gpio_array[0], Pin_array[0]) == 0){
				WAITFLAG = 0;				
				return STATUS_FIT;
			}
				
			k.CommState = 0x18;
			k.InputPower = 0;
			NorFlashWrite(NORFLASH_BALLAST_BASE + addr * NORFLASH_SECTOR_SIZE, (const short *)&k, (sizeof(Lightparam) + 1) / 2);
			tmp[0] = hextable[k.CommState >> 4];
			tmp[1] = hextable[k.CommState & 0x0F];
			ret = ZigbtaskApplyMemory(38 + 5);
			memset(ret, '0', 43);
			memcpy(ret, build, 4);
			memcpy((ret + 4), (dat + 3), 4);
			memcpy((ret + 4 + 4 + 2), tmp, 2);
			ret[38 + 4] = 0;
			
			buf = ProtocolRespond(g.GWAddr, (unsigned char *)(dat + 7), (const char *)ret, &size);
			GsmTaskSendTcpData((const char *)buf, size);
			ZigbTaskFreeMemory(buf);
			ZigbTaskFreeMemory(ret);
			WAITFLAG = 0;
			return COM_FAIL;
		}
	}
	return HANDLE_ERROR;
}

static void SZ05SendChar(char c) {
	USART_SendData(COMx, c);
	while (USART_GetFlagStatus(COMx, USART_FLAG_TXE) == RESET);
}

void SZ05SendString(char *str){
	while(*str){
		SZ05SendChar(*str++);
	}
}

extern unsigned char *DataSendToBSN(unsigned char control[2], unsigned char address[4], const char *msg, int *size);

static void ZigbeeHandleLightParam(FrameHeader *header, unsigned char CheckByte, const char *p){
}

static void ZigbeeHandleStrategy(FrameHeader *header, unsigned char CheckByte, const char *p){
	
}

static void ZigbeeHandleLightDimmer(FrameHeader *header, unsigned char CheckByte, const char *p){
	
}

static void ZigbeeHandleLightOnOff(FrameHeader *header, unsigned char CheckByte, const char *p){
	
}

static char Have_Param_Flag = 0;
static short NumbOfRank = 0;
static char *Shift;
static char Number = 0;

short CallTransfer(void){
	return NumbOfRank;
}

char *SpaceShift(void){
	return Shift;
}

static void ZigbeeHandleReadBSNData(FrameHeader *header, unsigned char CheckByte, const char *p){
	int i, instd;
	unsigned char *buf, space[3], addr[5], SyncFlag[13], *msg, size, fitflag = 0;
	unsigned short *ret, fitcount = 0, compare;
	GMSParameter g;
	Lightparam k;

 // buf = DataFalgQueryAndChange(4, 0, 1);    /*查看是否是服务器发来查询指令*/
	ret = DataFalgQueryAndChange(1, 0, 1);
	while(*ret){
		fitcount++;
		sscanf((const char *)header, "%*1s%4s", addr);
		
#if defined(__HEXADDRESS__)	
		if(*ret == (unsigned short)strtol((const char *)addr, NULL, 16)){
#else
		if(*ret == (unsigned short)atoi((const char *)addr)){
#endif			
			fitflag = 1;
			break;
		}
		ret++;	
	}
	
	sscanf(p, "%*1s%4s", addr);
		
#if defined(__HEXADDRESS__)
	instd = strtol((const char *)addr, NULL, 16);
#else				
	instd = atoi((const char *)addr);
#endif
	NorFlashRead(NORFLASH_BALLAST_BASE + instd * NORFLASH_SECTOR_SIZE, (short *)&k, (sizeof(Lightparam) + 1) / 2);
	
	k.UpdataTime = RtcGetTime();

	NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&g, (sizeof(GMSParameter) + 1) / 2);
	
	if(fitflag) {                             /*服务器发来查询镇流器数据帧*/	
			char build[4] = {'B', '0' , '0' , '0'};	
			
			msg = ZigbtaskApplyMemory(34 + 9);
			memcpy(msg, build, 4);
			memcpy((msg + 4), header->AD, 4);
			memcpy((msg + 4 + 4), (p + sizeof(FrameHeader)), 34);
			msg[38 + 4] = 0;
			
			buf = ProtocolRespond(g.GWAddr, header->CT, (const char *)msg, &size);
			GsmTaskSendTcpData((const char *)buf, size);
			ZigbTaskFreeMemory(buf);
			ZigbTaskFreeMemory(msg);
			DataFalgQueryAndChange(1, fitcount - 1, 0);
			
			*ret |= 0x4000; 
			
			sscanf(p, "%*11s%2s", space);
			i = atoi((const char *)space);
			k.InputPower = i;
			
			sscanf(p, "%*21s%4s", SyncFlag);
			compare = atoi((const char *)SyncFlag);
				
			
	} else {                       /*网关轮询镇流器数据*/
		StrategyParam s;
		DateTime dateTime;
		uint32_t second;
		unsigned char time_m, time_d, time_w;
		
		sscanf(p, "%*43s%12s", SyncFlag);
		
		if((k.Loop <= '8') || (k.Loop >= '0')) {
			Have_Param_Flag = 1;
		}

		if((strncmp((const char *)k.TimeOfSYNC, (const char *)SyncFlag, 12) != 0) && (Have_Param_Flag == 1)){     /*灯参数同步标识比较*/	
			msg = DataFalgQueryAndChange(5, 0, 1);
			while(*msg == 1){
				vTaskDelay(configTICK_RATE_HZ / 50);
			}
			DataFalgQueryAndChange(2, 5, 0);
			DataFalgQueryAndChange(3, 1, 0);
			DataFalgQueryAndChange(5, 1, 0);
			
			NumbOfRank = instd;
		}

		Have_Param_Flag = 0;
		sscanf(p, "%*55s%12s", SyncFlag);
		NorFlashRead(NORFLASH_STRATEGY_BASE + instd * NORFLASH_SECTOR_SIZE, (short *)&s, (sizeof(StrategyParam) + 1) / 2);
		
		if((s.SYNCTINE[0] <= '9') && (s.SYNCTINE[0] >= '0')) {
			Have_Param_Flag = 1;
		}

		if((strncmp((const char *)s.SYNCTINE, (const char *)SyncFlag, 12) != 0) && (Have_Param_Flag == 1)){     /*策略同步标识比较*/
			msg = DataFalgQueryAndChange(5, 0, 1);
			while(*msg == 1){
				vTaskDelay(configTICK_RATE_HZ / 50);
			}
			DataFalgQueryAndChange(2, 6, 0);
			DataFalgQueryAndChange(3, 1, 0);
			DataFalgQueryAndChange(5, 1, 0);
			
			NumbOfRank = instd;
			
		}
		
		sscanf(p, "%*67s%2s", SyncFlag);
		time_m = atoi((const char *)SyncFlag);
		
		sscanf(p, "%*69s%2s", SyncFlag);
		time_d = atoi((const char *)SyncFlag);
		
		sscanf(p, "%*71s%2s", SyncFlag);
		time_w = atoi((const char *)SyncFlag);
		
		second = RtcGetTime();
		SecondToDateTime(&dateTime, second);
		if((dateTime.month != time_m) || (dateTime.date != time_d) || (dateTime.week != time_w)){    /*镇流器时间对照*/
			msg = DataFalgQueryAndChange(5, 0, 1);
			while(*msg == 1){
				vTaskDelay(configTICK_RATE_HZ / 50);
			}
			DataFalgQueryAndChange(2, 7, 0);
			DataFalgQueryAndChange(3, 1, 0);
			DataFalgQueryAndChange(5, 1, 0);	
			NumbOfRank = instd;			
		
		}
		
		sscanf(p, "%*11s%2s", space);
		i = atoi((const char *)space);
		
		sscanf(p, "%*21s%4s", SyncFlag);
		if((SyncFlag[0] == '0') && (SyncFlag[1] == '0') && (SyncFlag[2] == '0') && (SyncFlag[3] == '0')){
			compare = 0;
		} else {
			compare = atoi((const char *)SyncFlag);
		}
		
		if((k.CommState != i) || (k.InputPower > (compare + 15)) || ((k.InputPower + 15) < compare)){   /*当前输入功率与上次功率的比较*/  /*当前工作状态与上次的状态比较*/
			msg = DataFalgQueryAndChange(5, 0, 1);
			while(*msg == 1){
				vTaskDelay(configTICK_RATE_HZ / 50);
			}
			DataFalgQueryAndChange(2, 8, 0);
			DataFalgQueryAndChange(3, 1, 0);
			DataFalgQueryAndChange(5, 1, 0);
			
			Shift = ZigbtaskApplyMemory(77);

			strcpy(Shift, p);

			NumbOfRank = instd;
			
			k.CommState = i;
			k.InputPower = compare;
			
			NorFlashWrite(NORFLASH_BALLAST_BASE + instd * NORFLASH_SECTOR_SIZE, (const short *)&k, (sizeof(Lightparam) + 1) / 2);
			
		}
		
		if(((k.InputPower > (compare + 3)) && (k.InputPower < (compare + 15))) ||
			 (((k.InputPower + 3) < compare) && ((k.InputPower + 15) > compare))){
			Number++;	 
			if(Number < 3){
				return;
			}
			Number = 0;
			msg = DataFalgQueryAndChange(5, 0, 1);
			while(*msg == 1){
				vTaskDelay(configTICK_RATE_HZ / 50);
			}
			DataFalgQueryAndChange(2, 8, 0);
			DataFalgQueryAndChange(3, 1, 0);
			DataFalgQueryAndChange(5, 1, 0);
			
			Shift = ZigbtaskApplyMemory(77);

			strcpy(Shift, p);

			NumbOfRank = instd;
			
			k.InputPower = compare;
			
			NorFlashWrite(NORFLASH_BALLAST_BASE + instd * NORFLASH_SECTOR_SIZE, (const short *)&k, (sizeof(Lightparam) + 1) / 2);	 			 
		}
	}
}

static void ZigbeeHandleLightAuto(FrameHeader *header, unsigned char CheckByte, const char *p){
	
}

static void ZigbeeHandleBSNUpgrade(FrameHeader *header, unsigned char CheckByte, const char *p){
	
}

extern char HexToAscii(char hex);

typedef void (*ZigbeeHandleFunction)(FrameHeader *header, unsigned char CheckByte, const char *p);
typedef struct {
	unsigned char type[2];
	ZigbeeHandleFunction func;
}ZigbeeHandleMap;

void __handleIOTRecieve(ZigbTaskMsg *p) {
	int i;
//	char *ret;
//	char verify = 0;
	char abNormal = 0;
	FrameHeader h;
	const char *dat = __ZigbGetMsgData(p);
		
	const static ZigbeeHandleMap map[] = {  
		{"02",  ZigbeeHandleLightParam},       /*0x02; 灯参数下载*/             /// 
		{"03",  ZigbeeHandleStrategy},         /*0x03; 策略下载*/               ///
		{"04",  ZigbeeHandleLightDimmer},      /*0x04; 灯调光控制*/
		{"05",  ZigbeeHandleLightOnOff},       /*0x05; 灯开关控制*/
		{"06",  ZigbeeHandleReadBSNData},      /*0x06; 读镇流器数据*/
		{"0A",  ZigbeeHandleLightAuto},        /*0x0A; 灯自动运行*/
		{"2A",  ZigbeeHandleBSNUpgrade},       /*0x2A; 镇流器远程升级*/            
	};
	
	h.FH = 0x02;
	sscanf(dat, "%*1s%4s", h.AD);
	sscanf(dat, "%*5s%2s", h.CT);
	sscanf(dat, "%*7s%2s", h.LT);
	if(h.CT[0] > '9'){
		h.CT[0] = h.CT[0] - '7';
	} else {
		h.CT[0] = h.CT[0] - '0';
	}
	h.CT[0] = HexToAscii(h.CT[0] - 8);

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if(strncmp((char *)&(h.CT), (char *)&(map[i].type[0]), 2) == 0){
			map[i].func(&h, abNormal, dat);
			break;
		}	
	}
}

char SendStatus(void){
	return WAITFLAG;
}

void __handleIOTSend(ZigbTaskMsg *p){
	int i;
	char *dat = __ZigbGetMsgData(p);

	for(i = 0; i < p->length; i++){
		SZ05SendChar(*dat++);
	}
}

typedef struct {
	ZigbTaskMsgType type;
	void (*handlerFunc)(ZigbTaskMsg *);
} HandlerOfMap;

static const HandlerOfMap __HandlerOfMaps[] = {
	{ TYPE_IOT_RECIEVE_DATA, __handleIOTRecieve},
	{ TYPE_IOT_SEND_DATA, __handleIOTSend},
	{ TYPE_IOT_NONE, NULL },
};

void ZigbeeHandler(ZigbTaskMsg *p) {
	const HandlerOfMap *map = __HandlerOfMaps;
	for (; map->type != TYPE_IOT_NONE; ++map) {
		if (p->type == map->type) {
			map->handlerFunc(p);
			break;
		}
	}
}

static void ZIGBEETask(void *parameter) {
	portBASE_TYPE rc;
	ZigbTaskMsg *message;
	for (;;) {
	//	printf("ZIGBEE: loop again\n");
		rc = xQueueReceive(__ZigbeeQueue, &message, portMAX_DELAY);
		if (rc == pdTRUE) {
			ZigbeeHandler(message);
			ZigbTaskFreeMemory(message);
			message = NULL;
		}
	}
}

extern void ProtocolInit(void);

void SHUNCOMInit(void) {
	SZ05_ADV_Init();
//	Time_Config();
	Zibee_Baud_CFG(9600);
	ProtocolInit();
	if (__Zigbeesemaphore == NULL) {
		vSemaphoreCreateBinary(__Zigbeesemaphore);
	}
	__ZigbeeQueue = xQueueCreate(100, sizeof(ZigbTaskMsg *));
	xTaskCreate(ZIGBEETask, (signed portCHAR *) "ZIGBEE", ZIGBEE_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}
