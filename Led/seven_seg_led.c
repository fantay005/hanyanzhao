#include <string.h>
#include "stm32f10x_gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "seven_seg_led.h"

static char __displayChar[SEVEN_SEG_LED_NUM];
static char __changed;
static xSemaphoreHandle __semaphore = NULL;

#define COMMON_IS_ANODE 1 // 1-π≤—Ù; 0-π≤“ı;

#define CHANNEL0_DATA_GPIO_PORT  GPIOF
#define CHANNEL0_DATA_GPIO_PIN   GPIO_Pin_7
#define CHANNEL1_DATA_GPIO_PORT GPIOE
#define CHANNEL1_DATA_GPIO_PIN  GPIO_Pin_2
#define CLK245_GPIO_PORT GPIOF
#define CLK245_GPIO_PIN  GPIO_Pin_8
#define LAUNCH_GPIO_PORT  GPIOE
#define LAUNCH_GPIO_PIN   GPIO_Pin_6

static inline void  __channel0SetDataHigh(void) {
	GPIO_SetBits(CHANNEL0_DATA_GPIO_PORT, CHANNEL0_DATA_GPIO_PIN);
}

static inline void __channel0SetDataLow(void) {
	GPIO_ResetBits(CHANNEL0_DATA_GPIO_PORT, CHANNEL0_DATA_GPIO_PIN);
}

static inline void  __channel1SetDataHigh(void) {
	GPIO_SetBits(CHANNEL1_DATA_GPIO_PORT, CHANNEL1_DATA_GPIO_PIN);
}

static inline void __channel1SetDataLow(void) {
	GPIO_ResetBits(CHANNEL1_DATA_GPIO_PORT, CHANNEL1_DATA_GPIO_PIN);
}

static inline void __setClkHigh(void) {
	GPIO_SetBits(CLK245_GPIO_PORT, CLK245_GPIO_PIN);
}

static inline void __setClkLow(void) {
	GPIO_ResetBits(CLK245_GPIO_PORT, CLK245_GPIO_PIN);
}

static inline void __setLaunchHigh(void) {
	GPIO_SetBits(LAUNCH_GPIO_PORT, LAUNCH_GPIO_PIN);
}

static inline void __setLaunchLow(void) {
	GPIO_ResetBits(LAUNCH_GPIO_PORT, LAUNCH_GPIO_PIN);
}

static void __shiftByte(unsigned char c0, unsigned char c1) {
	unsigned int bit;
	for (bit = 0x01; bit != 0x100; bit = bit << 1) {
		__setClkLow();
		if (bit & c0) {
			__channel0SetDataHigh();
		} else {
			__channel0SetDataLow();
		}

		if (bit & c1) {
			__channel1SetDataHigh();
		} else {
			__channel1SetDataLow();
		}
		__setClkHigh();
	}
}

void inline __display(void) {
	if (__changed) {
		int i;
		for (i = 0; i < sizeof(__displayChar) / 2; ++i) {
			__setLaunchLow();
			__shiftByte(__displayChar[i], __displayChar[i + sizeof(__displayChar) / 2]);
			__setLaunchHigh();
		}
		__changed = 0;
	}
}


void SevenSegLedInit(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	if (__semaphore != NULL) {
		return;
	}

	__changed = 1;

	vSemaphoreCreateBinary(__semaphore);

	__channel0SetDataHigh();
	__channel1SetDataHigh();
	__setClkHigh();
	__setLaunchHigh();

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;

	GPIO_InitStructure.GPIO_Pin = CHANNEL0_DATA_GPIO_PIN;
	GPIO_Init(CHANNEL0_DATA_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = CHANNEL1_DATA_GPIO_PIN;
	GPIO_Init(CHANNEL1_DATA_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = CLK245_GPIO_PIN;
	GPIO_Init(CLK245_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = LAUNCH_GPIO_PIN;
	GPIO_Init(LAUNCH_GPIO_PORT, &GPIO_InitStructure);

	__display();
}

static char __charToDisplayContent(uint8_t c) {
#if COMMON_IS_ANODE
	static const uint8_t displayTable[] = {
		(uint8_t)~0xBF, (uint8_t)~0x0B, (uint8_t)~0x77, (uint8_t)~0x5F,
		(uint8_t)~0xCB, (uint8_t)~0xDD, (uint8_t)~0xFD, (uint8_t)~0x0F,
		(uint8_t)~0xFF, (uint8_t)~0xDF, (uint8_t)~0x01, (uint8_t)~0x41
	};
#else

	static const uint8_t displayTable[] = { 0xBF, 0x0B, 0x77, 0x5F, 0xCB, 0xDD, 0xFD, 0x0F, 0xFF, 0xDF, 0x01, 0x41 };
#endif
	if (c >= sizeof(displayTable)) {
		return 0;
	}
	return displayTable[c];
}

bool SevenSegLedSetContent(unsigned int index, uint8_t what) {
	char content;

	if (index >= sizeof(__displayChar)) {
		return 0;
	}

	content = __charToDisplayContent(what);

	xSemaphoreTake(__semaphore, portMAX_DELAY);
	if (content != __displayChar[index]) {
		__displayChar[index] = content;
		__changed = 1;
	}
	xSemaphoreGive(__semaphore);

	return 1;
}


void SevenSegLedDisplay(void) {
	xSemaphoreTake(__semaphore, portMAX_DELAY);
	__display();
	xSemaphoreGive(__semaphore);
}
