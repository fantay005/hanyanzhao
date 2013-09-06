#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "protocol.h"
#include "misc.h"
#include "sms.h"
#include "xfs.h"


#define GSM_SET_RESET()  GPIO_SetBits(GPIOG, GPIO_Pin_10)
#define GSM_CLEAR_RESET()  GPIO_ResetBits(GPIOG, GPIO_Pin_10)
#define GSM_POWER_ON() GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define GSM_POWER_OFF() GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define HEART_BEAT_TIME  configTICK_RATE_HZ * 60 * 5;

static xQueueHandle __gsmTaskQueue;
static xQueueHandle __gsmUartQueue;


#define GsmPortMalloc(size) pvPortMalloc(size)
#define __GsmPortFree(p) vPortFree(p)

typedef enum {
	TYPE_USART_LINE = 0,
	TYPE_GPRS_DATA,
	TYPE_SEND_TCP_DATA,
	TYPE_RESET,
} GsmTaskMessageType;

typedef struct {
	GsmTaskMessageType type;
	unsigned int length;
} GsmTaskMessage;


static inline char *messageGetData(GsmTaskMessage *message) {
	return (char *)(&message[1]);
}

GsmTaskMessage *GsmCreateMessage(GsmTaskMessageType type, const char *dat, int len) {
	GsmTaskMessage *message = GsmPortMalloc(asizeof(GsmTaskMessage) + len);
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



//__inline void GsmPortFree(void *p) {
//	printf("Free\n");
//	vPortFree(p);
//}

//__inline void *GsmPortMalloc(int size) {
//	printf("Malloc\n");
//	return pvPortMalloc(size);
//}

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
	__gsmUartQueue = xQueueCreate(5, sizeof(GsmTaskMessage *));		  //队列创建

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

	GPIO_ResetBits(GPIOG, GPIO_Pin_10);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10 | GPIO_Pin_11;
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

	//----------------------------------------------------------------------------
	/* Enable the USART2 Interrupt 和手机模块通讯*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}


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
			GsmTaskMessage *message = GsmCreateMessage(TYPE_USART_LINE, buffer, bufferIndex);
			if (pdTRUE == xQueueSendFromISR((strncmp("+CMT", buffer, 4) == 0) ? __gsmTaskQueue : __gsmUartQueue, &message, &xHigherPriorityTaskWoken)) {
				if (xHigherPriorityTaskWoken) {
					taskYIELD();
				}
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

GsmTaskMessage *vATCommand(const char *cmd, const char *prefix, int timeoutTick) {
	GsmTaskMessage *message;
	portBASE_TYPE rc;
	UartQueueReset(__gsmUartQueue);

	while (*cmd) {
		USART_SendData(USART2, *cmd++);
		while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	}
	if (prefix == NULL) {
		vTaskDelay(timeoutTick);
		return NULL;
	}

	while (1) {
		rc = xQueueReceive(__gsmUartQueue, &message, timeoutTick);  // ???
		if (rc == pdFALSE) {
			break;
		}
		if ((message->type == TYPE_USART_LINE) &&
				(0 == strncmp(prefix, messageGetData(message), strlen(prefix)))) {
			return message;
		}
		GmsDestroyMessage(message);
	}
	return NULL;
}

int vATCommandCheck(const char *cmd, const char *prefix, int timeoutTick) {
	GsmTaskMessage *message = vATCommand(cmd, prefix, timeoutTick);
	GmsDestroyMessage(message);
	return (NULL != message);
}

int vATCommandCheckUntilOK(const char *cmd, const char *prefix, int timeoutTick, int times) {
	while (times-- > 0) {
		if (vATCommandCheck(cmd, prefix, timeoutTick)) {
			return 1;
		}
	}
	return 0;
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
	GsmTaskMessage *message;
	const char *p;
	while (1) {
		message = vATCommand("AT+QISTAT\r", "STATE:", configTICK_RATE_HZ * 2);
		if (message == NULL) {
			return 0;
		}

		if (message->type != TYPE_USART_LINE) {
			GmsDestroyMessage(message);
			vTaskDelay(configTICK_RATE_HZ);
			continue;
		}

		p = messageGetData(message);

		if (strncmp(&p[7], "CONNECT OK", 10) == 0) {
			GmsDestroyMessage(message);
			return 1;
		}
		if (strncmp(&p[7], "TCP CONNECTING", 12) == 0) {
			GmsDestroyMessage(message);
			vTaskDelay(configTICK_RATE_HZ);
			continue;
		}

		GmsDestroyMessage(message);
		break;
	}
	return 0;
}

int checkTcpAndConnect(const char *ip, unsigned short port) {
	char buf[44];
	GsmTaskMessage *message;
	if (isTcpConnected()) {
		return 1;
	}
	sprintf(buf, "AT+QIOPEN=\"TCP\",\"%s\",\"%d\"\r", ip, port);
	message = vATCommand(buf, "CONNECT", configTICK_RATE_HZ * 40);
	if (message == NULL) {
		return 0;
	}

	if (message->type != TYPE_USART_LINE) {
		GmsDestroyMessage(message);
		return 0;
	}

	if (strncmp("CONNECT OK", messageGetData(message), 10) == 0) {
		GmsDestroyMessage(message);
		return 1;
	}

	GmsDestroyMessage(message);
	return 0;
}

int sendTcpData(const char *p, int len) {
	int i;
	portBASE_TYPE rc;
	GsmTaskMessage *message;
	char buf[16];
	sprintf(buf, "AT+QISEND=%d\r", len);
	UartQueueReset(__gsmUartQueue);   // memory leak
	vATCommand(buf, NULL, configTICK_RATE_HZ / 5);
	for (i = 0; i < len; i++) {
		USART_SendData(USART2, *p++);
		while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	}
	while (1) {
		rc = xQueueReceive(__gsmUartQueue, &message, configTICK_RATE_HZ * 2); // ???
		if (rc == pdFALSE) {
			return 0;
		}

		if (message->type != TYPE_USART_LINE) {
			GmsDestroyMessage(message);
		}


		if (0 == strncmp(messageGetData(message), "SEND OK", 7)) {
			GmsDestroyMessage(message);
			return 1;
		} else if (0 == strncmp(messageGetData(message), "SEND FAIL", 9)) {
			GmsDestroyMessage(message);
			return 0;
		} else if (0 == strncmp(messageGetData(message), "ERROR", 5)) {
			GmsDestroyMessage(message);
			return 0;
		} else {
			GmsDestroyMessage(message);
		}
	}

}

int initGsmRuntime() {

	vATCommandCheck("AT\r", "OK", configTICK_RATE_HZ / 2);
	vATCommandCheck("AT\r", "OK", configTICK_RATE_HZ / 2);

	if (!vATCommandCheck("ATE0\r", "OK", configTICK_RATE_HZ)) {
		printf("ATE0  error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+IPR=19200\r", "OK", configTICK_RATE_HZ)) {
		printf("AT+IPR error\r");
		return 0;
	}

	while (1) {
		int exit = 0;
		GsmTaskMessage *message;
		portBASE_TYPE rc = xQueueReceive(__gsmUartQueue, &message, configTICK_RATE_HZ * 20);
		if (rc == pdTRUE) { // 收到数据
			if (message->type == TYPE_USART_LINE) {
				printf("Gsm: got data => %s\n", messageGetData(message));
				if (strncmp("Call Ready", messageGetData(message), 10) == 0) {
					exit = 1;
				}
			}
			GmsDestroyMessage(message);
		}
		if (exit) {
			break;
		}
	}

	if (!vATCommandCheck("ATS0=3\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("ATS0=3 error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+CMGF=0\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+CMGF=0 error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+CNMI=2,2,0,0,0\r", "OK", configTICK_RATE_HZ * 2)) {
		printf("AT+CNMI error\r");
		return 0;
	}

	if (!vATCommandCheckUntilOK("AT+CPMS=\"SM\"\r", "+CPMS", configTICK_RATE_HZ * 3, 10)) {
		printf("AT+CPMS error\r");
		return 0;
	}

	if (!vATCommandCheck("AT&W\r", "OK", configTICK_RATE_HZ)) {
		printf("AT&W error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+GSN\r", "OK", configTICK_RATE_HZ / 2)) {
		printf("AT+GSN error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+CSQ\r", "+CSQ:", configTICK_RATE_HZ / 5)) {
		printf("AT+CSQ error\r");
		return 0;
	}


	if (!vATCommandCheck("AT+QIDEACT\r", "DEACT", configTICK_RATE_HZ / 5)) {
		printf("AT+QIDEACT error\r");
		return 0;
	}


	if (!vATCommandCheck("AT+QIHEAD=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIHEAD error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+QISHOWRA=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWRA error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+QISHOWPT=1\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QISHOWPT error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+QIFGCNT=0\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QIFGCNT error\r");
		return 0;
	}

	if (!vATCommandCheck("AT+QICSGP=1,\"CMNET\"\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT+QICSGP error\r");
		return 0;
	}

	return 1;
}

// 删除短信？、？？;



void handleSMS(char *p) {
	GsmTaskMessage *message;
	sms_t *sms;

	portBASE_TYPE rc = xQueueReceive(__gsmUartQueue, &message, configTICK_RATE_HZ);
	if (rc != pdTRUE) {
		return;
	}

	if (message->type == TYPE_USART_LINE) {
		sms = GsmPortMalloc(sizeof(sms_t));
		printf("Gsm: got sms => %s\n", messageGetData(message));
		Sms_DecodePdu(messageGetData(message), sms);
		printf("Gsm: sms_content=> %s\n", sms->sms_content);
		if (sms->encode_type == ENCODE_TYPE_GBK) {
			XfsTaskSpeakGBK(sms->sms_content, sms->content_len);
		} else {
			XfsTaskSpeakUCS2(sms->sms_content, sms->content_len);
		}
		__GsmPortFree(sms);
	}
	GmsDestroyMessage(message);
}

void handleRING(char *p) {
	GPIO_ResetBits(GPIOD, GPIO_Pin_2);
}

/*
void handleGPRS(char *p) {
	unsigned char *pp = (unsigned char *)p;
	int len = (pp[2] << 8) + pp[3];
	p[14 + len] = 0;

	printf("Gms: got GPRS(%d) => ", len);
	printf(&p[11]);
}
*/

void handleLABA(char *p) {
	GPIO_SetBits(GPIOD, GPIO_Pin_2);
}

typedef void (*HandlerFunction)(char *p);
typedef struct {
	const char *prefix;
	HandlerFunction func;
} HandlerMap;

static void handlerAutoReport(char *p) {
	int i;
	const static HandlerMap map[] = {
		{"+CMT", handleSMS},
		{"RING", handleRING},
		{"#H", ProtocolHandler},  //IPD12TCP:
		{"NO CARRIER", handleLABA},
	};

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (0 == strncmp(p, map[i].prefix, strlen(map[i].prefix))) {
			map[i].func(p);
			return;
		}
	}
	printf("UNKNOW prefix\n");
}

void vGsm(void *parameter) {
	GsmTaskMessage *message;
	portBASE_TYPE rc;
	portTickType now;
	int t = 1;
	initHardware();

	while (1) {
		printf("Gsm start\n");
		startGsm();
		if (initGsmRuntime()) {
			break;
		}
	}

	while (1) {
		if (0 == checkTcpAndConnect("221.130.129.72", 5555)) {
			continue;
		} else {
			int addr;
			const char *data = ProtoclCreatLogin(&addr);
			sendTcpData(data, addr);
			ProtocolDestroyMessage(data);
			break;
		}
	}
	for (;;) {
		printf("Gsm: loop again\n");
		now = xTaskGetTickCount();
		rc = xQueueReceive(__gsmTaskQueue, &message, t);
		if (rc == pdTRUE) {
			t -= xTaskGetTickCount() - now;
			if (t < 0) {
				t = 0;
			}
			if (message->type == TYPE_USART_LINE) {
				handlerAutoReport(messageGetData(message));
			} else if (message->type == TYPE_GPRS_DATA) {
				ProtocolHandler(messageGetData(message));
			} else if (message->type == TYPE_SEND_TCP_DATA) {
				sendTcpData(messageGetData(message), message->length);
			} else if (message->type == TYPE_RESET) {
				unsigned int len = message->length;
				if (len > 100) {
					len = 100;
				}
				vTaskDelay(configTICK_RATE_HZ * len);
				SoftReset();
			}
			GmsDestroyMessage(message);
		} else {
			t = HEART_BEAT_TIME;
			if (0 == checkTcpAndConnect("221.130.129.72", 5555)) {
				printf("Gsm: Connect TCP error\n");
			} else {
				int size;
				const char *dat = ProtoclCreateHeartBeat(&size);
				sendTcpData(dat, size);
				ProtocolDestroyMessage(dat);
			}
		}
	}
}

