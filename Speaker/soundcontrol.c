#include "soundcontrol.h"
#include "stm32f10x_gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"

static xSemaphoreHandle __semaphore = NULL;
static unsigned char __channelsEnable;

#define GSM_PIN GPIO_Pin_0
#define XFS_PIN GPIO_Pin_6
#define FM_PIN GPIO_Pin_2
#define MP3_PIN GPIO_Pin_1
#define ALL_PIN (GSM_PIN | XFS_PIN | FM_PIN | MP3_PIN)

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

	GPIO_ResetBits(GPIOD, GPIO_Pin_7);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
}

void SoundControlInit(void) {
	if (__semaphore == NULL) {
		vSemaphoreCreateBinary(__semaphore);
		initHardware();
	}
}

static inline void __takeEffect(void) {
    if(GPIO_ReadInputDataBit(GPIOE, GSM_PIN) == 1){
		return;
	}

	if(GPIO_ReadInputDataBit(GPIOE, XFS_PIN) == 1){
	   if (__channelsEnable & SOUND_CONTROL_CHANNEL_GSM){
	   	   GPIO_ResetBits(GPIOE, XFS_PIN);
		   GPIO_SetBits(GPIOE, GSM_PIN);
		   return;
	   }
	}

	if(GPIO_ReadInputDataBit(GPIOE, MP3_PIN) == 1){
	   if (__channelsEnable & SOUND_CONTROL_CHANNEL_GSM){
	   	   GPIO_ResetBits(GPIOE, MP3_PIN);
		   GPIO_SetBits(GPIOE, GSM_PIN);
		   return;
	   }

	   if (__channelsEnable & SOUND_CONTROL_CHANNEL_XFS){
	   	   GPIO_ResetBits(GPIOE, MP3_PIN);
		   GPIO_SetBits(GPIOE, XFS_PIN);
		   return;
	   }
	}

	if(GPIO_ReadInputDataBit(GPIOE, FM_PIN) == 1){
	   if (__channelsEnable & SOUND_CONTROL_CHANNEL_GSM){
	   	   GPIO_ResetBits(GPIOE, FM_PIN);
		   GPIO_SetBits(GPIOE, GSM_PIN);
		   return;
	   }

	   if (__channelsEnable & SOUND_CONTROL_CHANNEL_XFS){
	   	   GPIO_ResetBits(GPIOE, FM_PIN);
		   GPIO_SetBits(GPIOE, XFS_PIN);
		   GPIO_SetBits(GPIOD, GPIO_Pin_7);
		   return;
	   }

	   if (__channelsEnable & SOUND_CONTROL_CHANNEL_MP3){
	   	   GPIO_ResetBits(GPIOE, FM_PIN);
		   GPIO_SetBits(GPIOE, MP3_PIN);
		   return;
	   }
	}

	if (__channelsEnable & SOUND_CONTROL_CHANNEL_GSM) {
		GPIO_SetBits(GPIOE, GSM_PIN);
		return;	
	}else{
		GPIO_ResetBits(GPIOE, GSM_PIN);
	}
		
	if (__channelsEnable & SOUND_CONTROL_CHANNEL_XFS) {
		GPIO_SetBits(GPIOE, XFS_PIN);
		return;	
	}else{
     	GPIO_ResetBits(GPIOE, XFS_PIN);
	}
		
	if (__channelsEnable & SOUND_CONTROL_CHANNEL_MP3) {
		GPIO_SetBits(GPIOE, MP3_PIN);
		return;	
	}else{
     	GPIO_ResetBits(GPIOE, MP3_PIN);
	}	
		
	if (__channelsEnable & SOUND_CONTROL_CHANNEL_FM) {
		GPIO_SetBits(GPIOE, FM_PIN);
		return;	
	}else{
	  	GPIO_ResetBits(GPIOE, FM_PIN);
	}
}

void SoundControlSetChannel(uint32_t channels, bool isOn) {
	xSemaphoreTake(__semaphore, portMAX_DELAY);
	if (isOn) {
		__channelsEnable |= channels;
	} else {
		__channelsEnable &= ~channels;
	}
	__takeEffect();
	xSemaphoreGive(__semaphore);
}
