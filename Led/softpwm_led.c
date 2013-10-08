#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_gpio.h"

#define XFS_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE )


static xQueueHandle __queue;

static int __light = 0;


void SoftPWNLedSetColor(enum SoftPWNLedColor color) {
	switch (color) {
	case SoftPWNLedColorRed:
			__light = (1 << 4);
		break;

	case SoftPWNLedColorOrange:
		__light = (1 << 5);
		break;

	case SoftPWNLedColorBlue:
		__light = (1 << 6);
		break;

	case SoftPWNLedColorYellow:
		__light = (1 << 7);
		break;
	}
}

static void __softPWMTask(void *unused) {
	portBASE_TYPE rc;
	static int i = 0;
	char msg;

	while (1) {
		rc = xQueueReceive(__queue, &msg, 1);
		if (pdTRUE == rc) {
			;
		} else {
			if ((GPIOA->ODR & (0x0F << 4)) != (0x0F << 4)) {
				i = 0;
				GPIOA->BSRR = 0x0F << 4;
			} else {
				i++;
				if (i == 5) {
					GPIOA->BRR = __light;
				}
			}
		}
	}
}

void SoftPWMLedInit() {
	//TIM3==========================================
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	GPIO_SetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);


	__queue = xQueueCreate(3, sizeof(char));
	xTaskCreate(__softPWMTask, (signed portCHAR *) "PWM", XFS_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 20, NULL);

}
