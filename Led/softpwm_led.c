#include "softpwm_led.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_gpio.h"

static int __light = 0;
static xTaskHandle __taskHandler = NULL;

static void __softPWMTask(void *unused) {
	int i = 0;
	while (1) {
		if ((GPIOA->ODR & (0x0F << 4)) != (0x0F << 4)) {
			i = 0;
			GPIOA->BSRR = 0x0F << 4;
		} else {
			if (++i == 7) {
				GPIOA->BRR = __light;
			}
		}
		vTaskDelay(1);
	}
}

bool SoftPWNLedSetColor(enum SoftPWNLedColor color) {
	switch (color) {
	case SoftPWNLedColorRed:
			__light = (1 << 7);
		break;

	case SoftPWNLedColorOrange:
		__light = (1 << 4);
		break;

	case SoftPWNLedColorBlue:
		__light = (1 << 6);
		break;

	case SoftPWNLedColorYellow:
		__light = (1 << 5);
		break;

	case SoftPWNLedColorNULL:
		__light = 0;
		break;
	}

	if (__taskHandler == NULL && __light != 0) {
		portBASE_TYPE rc = xTaskCreate(__softPWMTask, (signed portCHAR *) "PWM", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 20, &__taskHandler);
		if (pdPASS != rc) {
			__taskHandler = NULL;
			return false;
		}
	}

	if (__taskHandler != NULL && __light == 0) {
		vTaskDelete(__taskHandler);
		__taskHandler = NULL;
	}

	return true;
}


void SoftPWMLedInit() {
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_SetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}
