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

#if 0
//------------------------------------------------------------------------------
static char __readByte(void) {
	unsigned char i, val = 0;

	__clkDataHigh();
	for (i = 0x80; 0 < i; i /= 2) {
		__clkSetHigh();
		__delayus(1);
		if (__readDataBit()) {
			val = (val | i);
		}
		__delayus(1);
		__clkSetLow();
		__delayus(1);
	}
	__clkDataLow();//结束数据传输
	__delayus(1);

	__clkSetHigh();
	__delayus(1);
	__clkSetLow();
	__delayus(1);
	__clkDataHigh();
	return val;
}
#endif

static char __writeByte(unsigned char value_w) {
	unsigned char i, error = 0;

	__clkDataHigh();
	__delayus(1);

	for (i = 0x80; 0 < i; i /= 2) {
		if (i & value_w) {
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

	__clkDataLow();     //写no ack
	__delayus(1);
	__clkSetHigh();
	__delayus(1);
	__clkSetLow();
	__delayus(1);

	return error;   //error=1 in case of no acknowledge

}

static int __readData(void) {
	unsigned char i, val = 0;
	unsigned char a, b;
	int m_org_data;

	__clkDataHigh();
	__clkSetLow();
	val = 0;

	__delayus(1);

	for (i = 0; i < 8; i++) {
		val = val << 1;
		__clkSetHigh();
		__delayus(1);

		if (__readDataBit() == 1) {
			val = (val | 0x01);  //read bit
		}
		__delayus(1);
		__clkSetLow();
		__delayus(1);
	}
	a = val;

	{
		__clkDataLow();      //MCU告诉传感器已经收到了一个字节
		__delayus(1);
		__clkSetHigh();
		__delayus(3);
		__clkSetLow();
		__delayus(1);
		__clkDataHigh();
	}

	val = 0;
	for (i = 0; i < 8; i++) {
		val = val << 1;
		__clkSetHigh();
		__delayus(1);

		if (__readDataBit() == 1) {
			val = (val | 0x01);  //read bit
		}
		__clkSetLow();
		__delayus(1);
	}
	b = val;

	{
		__clkDataLow();      //MCU告诉传感器已经收到了一个字节
		__delayus(1);
		__clkSetHigh();
		__delayus(1);
		__clkSetLow();
		__delayus(1);
		__clkDataHigh();
	}

	m_org_data = a;
	m_org_data = (m_org_data << 8);
	m_org_data |= b;

	return m_org_data;
}

//----------------------------------------------------------------------------------
// 返回值单位0.01摄氏度
static int __readTemperature(void) {
	unsigned int i;
	int temp;

	__resetConnection();
	__writeByte(0x03);

	vTaskDelay(configTICK_RATE_HZ * 320 / 1000);
	for (i = 0; i < 500; ++i) {
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

#if 0
static unsigned char __softReset(void) {
	unsigned char error = 0;
	__resetConnection();              //reset communication
	__startTransfer();
	error += __writeByte(0x1e);      //send RESET-command to sensor
	return error;                     //error=1 in case of no response form the sensor
}
#endif

//SHT^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//------------------------------------------------------------------------------
bool SHT10ReadTemperatureHumidity(int *temp, int *humi) {
	int t, h;
	xSemaphoreTake(__semaphore, portMAX_DELAY);
	t = __readTemperature(); //测量温度和湿度(float)
	h = __readHumidity(t);    //测量温度和湿度(float)
	xSemaphoreGive(__semaphore);

	if (temp) {
		*temp = t / 10;
	}

	if (humi) {
		*humi = h;
	}

	return true;
}
