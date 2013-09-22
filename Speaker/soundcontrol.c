#include "soundcontrol.h"
#include "stm32f10x_gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"

static xSemaphoreHandle __semaphore = NULL;
static unsigned char __channelsEnable; 

static void initHardware(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;

	GPIO_ResetBits(GPIOE, GPIO_Pin_0);
	GPIO_ResetBits(GPIOE, GPIO_Pin_1);
	GPIO_ResetBits(GPIOE, GPIO_Pin_2);
	GPIO_ResetBits(GPIOE, GPIO_Pin_6);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOC, GPIO_Pin_1);
	GPIO_ResetBits(GPIOC, GPIO_Pin_2);
	GPIO_ResetBits(GPIOC, GPIO_Pin_3);
	GPIO_ResetBits(GPIOC, GPIO_Pin_4);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void SoundControlInit(void) {
	if (__semaphore == NULL) {
		vSemaphoreCreateBinary(__semaphore);
		initHardware();
	}
}

static inline void __takeEffect(void) {
	if (__channelsEnable & (SOUND_CONTROL_CHANNEL_GSM |
							SOUND_CONTROL_CHANNEL_XFS |
							SOUND_CONTROL_CHANNEL_MP3 |
							SOUND_CONTROL_CHANNEL_FM)) {
			
	} else {
	}
}

void SoundControlSetChannel(uint32_t channels, bool isOn)
{
	xSemaphoreTake(__semaphore, portMAX_DELAY);
	if (isOn) {
		__channelsEnable |= channels;
	} else {
		__channelsEnable &= ~channels;
	}
	__takeEffect();
	xSemaphoreGive(__semaphore);
}
