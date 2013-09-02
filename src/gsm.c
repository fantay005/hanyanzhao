#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "string.h"
#include "stdio.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "misc.h"
#include "sms.h"
#include "xfs.h"

#define GSM_SET_RESET()  GPIO_SetBits(GPIOG, GPIO_Pin_10)
#define GSM_CLEAR_RESET()  GPIO_ResetBits(GPIOG, GPIO_Pin_10)
#define GSM_POWER_ON() GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define GSM_POWER_OFF() GPIO_ResetBits(GPIOB, GPIO_Pin_0)

static char *buffer;
static int bufferIndex = 0;
static xQueueHandle xQueue;


#if 0
void EXTI1_IRQHandler(void) {
	if (EXTI_GetITStatus(EXTI_Line1) == RESET) {
		return;
	}
	EXTI_ClearITPendingBit(EXTI_Line1);
}
#endif

void USART2_IRQHandler(void) {
	char data;

	if (USART_GetITStatus(USART2, USART_IT_RXNE) == RESET) {
		return;
	}

	data = USART_ReceiveData(USART2);
	USART_SendData(USART1, data);
	USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	if (data == '\n') {
		buffer[bufferIndex] = 0;
		if (bufferIndex >= 2) {
			portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
			if (pdTRUE == xQueueSendFromISR(xQueue, &buffer, &xHigherPriorityTaskWoken)) {
				buffer = pvPortMalloc(200);
				if (xHigherPriorityTaskWoken) {
					taskYIELD();
				}
			}
		}
		bufferIndex = 0;
	} else if (data != '\r') {
		buffer[bufferIndex++] = data;
	}
}

void UartQueueReset(xQueueHandle queue) {
	char *p;	
	while (pdTRUE == xQueueReceive(queue, &p, 0)) {
		vPortFree(p);
	}
}

char *vATCommand(const char *cmd, const char *prefix, int timeoutTick) {
	char *p;
	portBASE_TYPE rc;
	UartQueueReset(xQueue);  // memory leak
	while (*cmd) {
		USART_SendData(USART2, *cmd++);
		while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	}
	if (prefix == NULL) {
		vTaskDelay(timeoutTick);
		return NULL;
	}

	while (1) {
		rc = xQueueReceive(xQueue, &p, timeoutTick);  // ???
		if (rc == pdFALSE) {
			break;
		}
		if (0 == strncmp(prefix, p, strlen(prefix))) {
			return p;
		}
	}

	return NULL;
}

static void initHardware() {
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	xQueue = xQueueCreate(2, sizeof(char *));		  //队列创建

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

#if 0
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);				   //GSM模块的RI

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource1);
	EXTI_InitStructure.EXTI_Line = EXTI_Line1;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);
#endif

}


int vATCommandCheck(const char *cmd, const char *prefix, int timeoutTick) {
	char *p = vATCommand(cmd, prefix, timeoutTick);
	vPortFree(p);
	return (NULL != p);
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
	char *p;
	while (1) {
		p = vATCommand("AT+QISTAT\r", "STATE:", configTICK_RATE_HZ * 2);
		if (p == NULL) return 0;
		if (strncmp(&p[7], "CONNECT OK", 10) == 0) {
			vPortFree(p);
			return 1;
		}
		if (strncmp(&p[7], "TCP CONNECTING", 12) == 0) {
			vPortFree(p);
			vTaskDelay(configTICK_RATE_HZ);
			continue;
		}
		vPortFree(p);
		break;
	}
	return 0;	
}

int checkTcpAndConnect(const char *ip, unsigned short port) {
	char buf[44];
	char *p;
	if (isTcpConnected()) return 1;
	sprintf(buf, "AT+QIOPEN=\"TCP\",\"%s\",\"%d\"\r", ip, port);
	p = vATCommand(buf, "CONNECT", configTICK_RATE_HZ*2);
	if (p == NULL) {
		return 0;
	}

	if (strncmp("CONNECT OK", p, 10) == 0) {	
		vPortFree(p);
		return 1;
	}
	
	vPortFree(p);
	return 0;
}

int sendTcpData(const char *p, int len) {
	int i;
	portBASE_TYPE rc;
	char *rev;
	vATCommand("AT+QISEND\r", NULL, configTICK_RATE_HZ/5);
	for (i = 0; i < len; i++) {
		USART_SendData(USART2, *p++);
		while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);		
	}
	UartQueueReset(xQueue);   // memory leak
	USART_SendData(USART2, 0x1A);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	while (1) {
		rc = xQueueReceive(xQueue, &rev, configTICK_RATE_HZ*2);  // ???
		if (rc == pdFALSE) {
			return 0;
		}
		if (0 == strncmp(rev, "SEND OK", 7)) {		
			vPortFree(rev);
			return 1;
		} else if (0 == strncmp(rev, "SEND FAIL", 9)) {		
			vPortFree(rev);
			return 0;
		} else if (0 == strncmp(rev, "ERROR", 5)) {	
			vPortFree(rev);
			return 0;
		} else {	
			vPortFree(rev);
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

	if (!vATCommandCheck("AT&W\r", "OK", configTICK_RATE_HZ)) {
		printf("AT&W error\r");
		return 0;
	}

	while (1) {
		int exit = 0;
		char *p;
		portBASE_TYPE rc = xQueueReceive(xQueue, &p, configTICK_RATE_HZ * 20);
		if (rc == pdTRUE) { // 收到数据
			printf("Gsm: got data => %s\n", p);
			if (strncmp("Call Ready", p, 10) == 0) {
				exit = 1;
			}
			vPortFree(p);
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

	if (!vATCommandCheck("AT+GSN\r", "OK", configTICK_RATE_HZ / 5)) {
		printf("AT&W error\r");
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


	if (!vATCommandCheck("AT+QIHEAD=1\r", "OK", configTICK_RATE_HZ / 5)) {
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
	portBASE_TYPE rc = xQueueReceive(xQueue, &p, configTICK_RATE_HZ);
	if (rc == pdTRUE) {
		sms_t *sms = pvPortMalloc(sizeof(sms_t));
		printf("Gsm: got sms => %s\n", p);
		Sms_DecodePdu(p, sms);
		vPortFree(p);
		printf("Gsm: sms_content=> %s\n", sms->sms_content);
		xfsSpeak(sms->sms_content, sms->content_len, sms->encode_type == ENCODE_TYPE_GBK ? TYPE_GBK : TYPE_UCS2);
		vPortFree(sms);
	}
}

void handleRING(char *p) {
	printf("Gsm: got Ring\n");
}

void handleGPRS(char *p) {
	int len = atoi(&p[3]) - 2;
	int i;
	for (i = 0; i < 12; i++) {
		if (p[i] == ':') break;
	}
	if (i >= 12) return;

	printf("Gms: got GPRS => %s\n", &p[1 + i]);
}

typedef void (*HandlerFunction)(char *p);
typedef struct {
	const char *prefix;
	HandlerFunction func;
} HandlerMap;

static void handlerAutoReport(char *p) {
	int i;
	const HandlerMap map[] = {
		{"+CMT", handleSMS},
		{"RING", handleRING},
		{"IPD", handleGPRS},  //IPD12TCP:
	};

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (0 == strncmp(p, map[i].prefix, strlen(map[i].prefix))) {
			map[i].func(p);
			vPortFree(p);
		}
	}
}

#define HEART_BEAT_TIME  configTICK_RATE_HZ * 60 * 1;

void vGsm(void *parameter) {
	char *p;
	portBASE_TYPE rc;
	portTickType now;
	int t = 1;
	buffer = pvPortMalloc(200);
	initHardware();

	while (1) {
		printf("Gsm start\n");
		startGsm();
		if (initGsmRuntime()) {
			break;
		}
	}

	for (;;) {			
		printf("Gsm: loop again\n");
		now = xTaskGetTickCount();
		rc = xQueueReceive(xQueue, &p, t);
		if (rc == pdTRUE) { // 收到数据		
			t -= xTaskGetTickCount() - now;
			if (t < 0) t = 0;
			printf("Gsm: got data => %s\n", p);
			handlerAutoReport(p);
		} else {
			t = HEART_BEAT_TIME;
			if (0 == checkTcpAndConnect("61.190.61.78", 1550)) {
				printf("Gsm: Connect TCP error\n");
			} else {
				const char dat[] = "hello world\r\n";
				sendTcpData(dat, sizeof(dat));			
			}
		}
	}
}

