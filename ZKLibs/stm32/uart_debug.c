#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "misc.h"


#define DEBUG_TASK_STACK_SIZE		( configMINIMAL_STACK_SIZE + 512)

//#define TEST_PIN       GPIO_Pin_1

#define PIN_PRINT_TX   GPIO_Pin_12   //USART1  GPIO_Pin_9     //USART2  GPIO_Pin_2
#define PIN_PRINT_RX   GPIO_Pin_2   //USART1  GPIO_Pin_10    //USART2  GPIO_Pin_3
#define GPIO_TX        GPIOC
#define GPIO_RX        GPIOD

#define COM_PRINT      UART5
#define COMM_IRQn      UART5_IRQn

static xQueueHandle __uartDebugQueue;

static inline void __uartDebugHardwareInit(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef   USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	GPIO_InitStructure.GPIO_Pin =  PIN_PRINT_TX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_TX, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_PRINT_RX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIO_RX, &GPIO_InitStructure);
	
//	GPIO_InitStructure.GPIO_Pin = TEST_PIN;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
//	GPIO_Init(GPIO_PRINT, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(COM_PRINT, &USART_InitStructure);
	USART_ITConfig(COM_PRINT, USART_IT_RXNE, ENABLE);
	USART_Cmd(COM_PRINT, ENABLE);
	
//	USART_ClearFlag(COM_PRINT, USART_FLAG_TC); 
  USART_ClearFlag(COM_PRINT, USART_FLAG_TXE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

	NVIC_InitStructure.NVIC_IRQChannel = COMM_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}


static void __uartDebugTask(void *nouse) {
	portBASE_TYPE rc;
	char *msg;

	printf("UartDebugTask: start\n");
	__uartDebugQueue = xQueueCreate(10, sizeof(char *));
	while (1) {
		rc = xQueueReceive(__uartDebugQueue, &msg, portMAX_DELAY);
		if (rc == pdTRUE) {
			extern void DebugHandler(char * msg);
			DebugHandler(msg);
			vPortFree(msg);
		}
	}
}

static uint8_t *__uartDebugCreateMessage(const uint8_t *dat, int len) {
	uint8_t *r = pvPortMalloc(len);
	memcpy(r, dat, len);
	return r;
}

static inline void __uartDebugCreateTask(void) {
	xTaskCreate(__uartDebugTask, (signed portCHAR *) "DBG", DEBUG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
}

void UartDebugInit() {
	__uartDebugHardwareInit();
	__uartDebugCreateTask();
}

void UART5_IRQHandler(void) {
	static uint8_t buffer[255];
	static int index = 0;

	uint8_t dat;
	if (USART_GetITStatus(COM_PRINT, USART_IT_RXNE) != RESET) {
		dat = USART_ReceiveData(COM_PRINT);
//		USART_SendData(COM_PRINT, dat);
		USART_ClearITPendingBit(COM_PRINT, USART_IT_RXNE);
		if (dat == '\r' || dat == '\n') {
			uint8_t *msg;
			portBASE_TYPE xHigherPriorityTaskWoken;

			buffer[index++] = 0;
			msg = __uartDebugCreateMessage(buffer, index);
			if (pdTRUE == xQueueSendFromISR(__uartDebugQueue, &msg, &xHigherPriorityTaskWoken)) {
				if (xHigherPriorityTaskWoken) {
					portYIELD();
				}
			}
			index = 0;

		} else {
			buffer[index++] = dat;
		}
	}
}


int fputc(int c, FILE *f) {
	while (USART_GetFlagStatus(COM_PRINT, USART_FLAG_TXE) == RESET);
	USART_SendData(COM_PRINT, c);
	return c;
}

int putch(int c) {
	while (USART_GetFlagStatus(COM_PRINT, USART_FLAG_TXE) == RESET);
	USART_SendData(COM_PRINT, c);
	return c;
}

