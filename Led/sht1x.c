#include "sht1x.h"
#include "stm32f10x_gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_rtc.h"

#define CLK_GPIO_PORT GPIOB
#define CLK_GPIO_PIN  GPIO_Pin_12

#define DAT_GPIO_PORT GPIOB
#define DAT_GPIO_PIN  GPIO_Pin_13

static inline void __clkSetLow(void) {
	GPIO_ResetBits(CLK_GPIO_PORT, CLK_GPIO_PIN);
}
static inline void __clkSetHigh(void) {
	GPIO_SetBits(CLK_GPIO_PORT, CLK_GPIO_PIN);
}

static inline void __clkDataLow(void) {
	GPIO_ResetBits(DAT_GPIO_PORT, DAT_GPIO_PIN);
}
static inline void __clkDataHigh(void) {
	GPIO_SetBits(DAT_GPIO_PORT , DAT_GPIO_PIN);
}
static inline int __readDataBit(void) {
	return GPIO_ReadInputDataBit(DAT_GPIO_PORT, DAT_GPIO_PIN);
}

static xSemaphoreHandle __semaphore = NULL;

void SHT10Init(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	if (__semaphore != NULL) {
		return;
	}

	vSemaphoreCreateBinary(__semaphore);

	GPIO_InitStructure.GPIO_Pin = CLK_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(CLK_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = DAT_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(DAT_GPIO_PORT, &GPIO_InitStructure);

	__clkSetLow();
	__clkDataLow();
}


static void __delayus(unsigned int t) {
	unsigned int i;
	for (i = 0; i < t; ++i);
}

static void __startTransfer(void) {
	__clkDataHigh();    //__delayus(1);
	__clkSetLow();
	__delayus(1);              //Initial state
	__clkSetHigh();
	__delayus(1);

	__clkDataLow();
	__delayus(1);
	__clkSetLow();
	__delayus(1);
	__clkSetHigh();
	__delayus(1);

	__clkDataHigh();
	__delayus(1);
	__clkSetLow();
	__delayus(1);
}

static void __resetConnection(void) {
	unsigned char i;

	__clkDataHigh();
	__delayus(1);

	__clkDataHigh();
	__clkSetLow();
	__delayus(1);
	for (i = 0; i < 9; i++) { // 9 SCK cycles
		__clkSetHigh();
		__delayus(1);
		__clkSetLow();
		__delayus(1);
	}

	__startTransfer();
}

static void __writeByte(uint8_t val) {
	uint8_t bit;

	__clkDataHigh();
	__delayus(1);

	for (bit = 0x80; bit != 0; bit = bit >> 1) {
		if (bit & val) {
			__clkDataHigh();
		} else {
			__clkDataLow();
		}
		__delayus(1);
		__clkSetHigh();
		__delayus(1);
		__clkSetLow();
		__delayus(1);
	}

	__clkDataLow();
	__delayus(1);
	__clkSetHigh();
	__delayus(1);
	__clkSetLow();
	__delayus(1);
}


static uint16_t __readByte(void) {
	uint8_t bit, val;

	__clkSetLow();
	__clkDataHigh();
	__delayus(1);

	for (val = 0, bit = 0x80; bit != 0; bit = bit >> 1) {
		__clkSetHigh();
		__delayus(1);

		if (__readDataBit() == 1) {
			val |= bit;
		}
		__clkSetLow();
		__delayus(1);
	}
	__clkDataLow();      //MCU告诉传感器已经收到了一个字节
	__delayus(1);
	__clkSetHigh();

	return val;
}

static uint16_t __readData(void) {
	uint16_t ret;
	ret = __readByte();
	ret = ret << 8;
	__delayus(2);
	ret += __readByte();
	return ret;
}

static int __readTemperature(void) {
	unsigned int i;
	int temp;

	__resetConnection();
	__writeByte(0x03);

	vTaskDelay(configTICK_RATE_HZ * 320 / 1000);
	for (i = 0; i < 50; ++i) {
		if (__readDataBit() == 0) {
			break;
		}
		vTaskDelay(configTICK_RATE_HZ / 10);
	}

	temp = __readData();     //会得到origin_temp
	temp = temp - 4000;         //calc. temperature from ticks to [%C]
	//t=d1+d2*SOt
	if (temp > 6000) {
		temp = 6000;    //cut if the value is outside of
	}
	if (temp < -900) {
		temp = -900;    //the physical possible range
	}
	return temp;
}

//----------------------------------------------------------------------------------
static int __readHumidity(int temp) {
	unsigned int i;
	int humi;

	__resetConnection();
	__writeByte(0x05);

	vTaskDelay(configTICK_RATE_HZ * 400 / 1000);
	for (i = 0; i < 500; ++i) {
		if (__readDataBit() == 0) {
			break;
		}
		vTaskDelay(configTICK_RATE_HZ / 10);
	}

	humi = __readData();
//	temp_c = 0.01 * origin_temp - 40;
//	rh_lin = humi*0.0405 - 0.0000028*humi*humi - 4.0;
//	rh_true = rh_lin + (temp_c-25)*(0.01+0.00008*humi);

//	rh_true = (rh_lin * 100) + (temp - 2500) * (0.01 + 0.00008 * humi); // *100
//	rh_true = (rh_lin * 100*100000) + (temp - 2500) * (1000 + 8 * humi); // *100*100000
//	rh_true = ((humi * 0.0405 - 0.0000028 * humi * humi - 4.0) * 100000000) + (temp - 2500) * (1000 + 8 * humi); // *100*100000
//	rh_true = ((humi * 405000 - 28 * humi * humi - 40000000)) + (temp - 2500) * (1000 + 8 * humi); // *100*100000
//	rh_true = rh_true / (1000 * 10000);
	humi = ((humi * 405000 - 28 * humi * humi - 40000000)) + (temp - 2500) * (1000 + 8 * humi); // *100*100000
	humi = humi / (100 * 10000);

	if (humi > 999) {
		humi = 999;
	}
	if (humi < 1) {
		humi = 1;
	}

	return humi;
}

bool SHT10ReadTemperatureHumidity(int *temp, int *humi) {
	int t, h;
	xSemaphoreTake(__semaphore, portMAX_DELAY);
	t = __readTemperature();
	h = __readHumidity(t);
	xSemaphoreGive(__semaphore);

	if (temp) {
		*temp = t / 10;
	}

	if (humi) {
		*humi = h;
	}

	return true;
}
