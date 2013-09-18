//2012-11-14
//Temperature, humidity measure in air
//PC.14 SHT_DAT
//PC.15 SHT_CLK
//Maximum sck is 10MHz
//SHT-------------------------------------------------------

#include "sht1x.h"
#include "stm32f10x_gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define CLK_GPIO_PORT GPIOB
#define CLK_GPIO_PIN  GPIO_Pin_12
#define DAT_GPIO_PORT GPIOB
#define DAT_GPIO_PIN  GPIO_Pin_13


//土壤温湿度传感器也是SHT10
#define sht_clk_0  GPIO_ResetBits(CLK_GPIO_PORT,CLK_GPIO_PIN)
#define sht_clk_1  GPIO_SetBits(CLK_GPIO_PORT, CLK_GPIO_PIN)
#define sht_dat_0  GPIO_ResetBits(DAT_GPIO_PORT, DAT_GPIO_PIN)
#define sht_dat_1  GPIO_SetBits(DAT_GPIO_PORT ,DAT_GPIO_PIN)
#define sht_dat_r  GPIO_ReadInputDataBit(DAT_GPIO_PORT, DAT_GPIO_PIN) //读出数据线数据

static xSemaphoreHandle __semaphore = NULL;

void SHT11Init(void) {
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

	sht_clk_0;
	sht_dat_0;

}


void __delayus(unsigned int t) {
	unsigned int i;
	for (i = 0; i < t; ++i);
}

// generates a transmission start
//       _____         ________
// DATA:      |_______|
//           ___     ___
// SCK : ___|   |___|   |______
void __startTransfer(void) {
	//DDRD |= 0x08;       //data都设为输出;CLK一直是输出,没必要再设一遍
	//WriteState();

	sht_dat_1;    //__delayus(1);
	sht_clk_0;
	__delayus(1);              //Initial state
	sht_clk_1;
	__delayus(1);

	sht_dat_0;
	__delayus(1);
	sht_clk_0;
	__delayus(1);
	sht_clk_1;
	__delayus(1);

	sht_dat_1;
	__delayus(1);
	sht_clk_0;
	__delayus(1);
}
//------------------------------------------------------------------------------
// communication reset: DATA-line=1 and at least 9 SCK cycles followed by transstart
//       _____________________________________________________         ________
// DATA:                                                      |_______|
//          _    _    _    _    _    _    _    _    _        ___     ___
// SCK : __| |__| |__| |__| |__| |__| |__| |__| |__| |______|   |___|   |______

void __resetConnection(void) {
	unsigned char i;

	sht_dat_1;
	__delayus(1);

	sht_dat_1;
	sht_clk_0;
	__delayus(1);
	for (i = 0; i < 9; i++) { // 9 SCK cycles
		sht_clk_1;
		__delayus(1);
		sht_clk_0;
		__delayus(1);
	}

	__startTransfer();
}
//------------------------------------------------------------------------------
char __readByte(void) {
	unsigned char i, val = 0;

	sht_dat_1;
	for (i = 0x80; 0 < i; i /= 2) {
		sht_clk_1;
		__delayus(1);
		if (sht_dat_r) {
			val = (val | i);
		}
		__delayus(1);
		sht_clk_0;
		__delayus(1);
	}
	sht_dat_0;//结束数据传输
	__delayus(1);

	sht_clk_1;
	__delayus(1);
	sht_clk_0;
	__delayus(1);
	sht_dat_1;
	return val;
}
//------------------------------------------------------------------------------
// writes a byte on the Sensibus and checks the acknowledge
//       __                         _____     _____    ________
// DATA:   |_______________________|     |___|     |__|
//           _    _    _    _    _    _    _    _    _
// SCK : ___| |__| |__| |__| |__| |__| |__| |__| |__| |______
//          a2   a1   a0   c4   c3   c2   c1   c0   ack
char __writeByte(unsigned char value_w) {
	//unsigned char i,t,error=0;
	unsigned char i, error = 0;
	//unsigned char ack_state2=0;

	//开始传输
	sht_dat_1;
	__delayus(1);

	/*
	tehu_dat_1;    __delayus(8);
	tehu_clk_0;    __delayus(8);              //Initial state
	tehu_clk_1;    __delayus(8);

	tehu_dat_0;    __delayus(8);
	tehu_clk_0;    __delayus(8);
	tehu_clk_1;    __delayus(8);

	tehu_dat_1;	   __delayus(8);
	tehu_clk_0;	   __delayus(8);
	*/

	//写控制命令
	for (i = 0x80; 0 < i; i /= 2) {
		if (i & value_w) {
			sht_dat_1;
		} else {
			sht_dat_0;
		}
		__delayus(1);
		sht_clk_1;
		__delayus(1);
		sht_clk_0;
		__delayus(1);
	}

	sht_dat_0;     //写no ack
	__delayus(1);
	sht_clk_1;
	__delayus(1);
	sht_clk_0;
	__delayus(1);

	//tehu_dat_1;
	//ReadState();
	//__delayus(8);

	return error;   //error=1 in case of no acknowledge

}

//----------------------------------------------------------------------------------
// reads a byte form the Sensibus and gives an acknowledge in case of "ack=1"
//       __                         _____     _____    ________
// DATA:   |_______________________|     |___|     |__|
//           _    _    _    _    _    _    _    _    _
// SCK : ___| |__| |__| |__| |__| |__| |__| |__| |__| |______
//          a2   a1   a0   c4   c3   c2   c1   c0   ack

int __readData(void)

{
	//unsigned char i,val=0,temp=0;
	unsigned char i, val = 0;
	unsigned char a, b;
	int m_org_data;

	sht_dat_1;
	sht_clk_0;
	val = 0;

	__delayus(1);
	//读取高8位(最高4位是0)

	for (i = 0; i < 8; i++) {
		val = val << 1;
		sht_clk_1;
		__delayus(1);

		if (sht_dat_r == 1) {
			val = (val | 0x01);  //read bit
		}
		__delayus(1);
		sht_clk_0;
		__delayus(1);
	}
	a = val;

	//MCU拉低data,告诉HT11已经收到了一个字节
	{
		sht_dat_0;      //MCU告诉传感器已经收到了一个字节
		__delayus(1);
		sht_clk_1;
		__delayus(3);
		sht_clk_0;
		__delayus(1);
		sht_dat_1;
	}

	//读取低8位
	val = 0;
	for (i = 0; i < 8; i++) {
		val = val << 1;
		sht_clk_1;
		__delayus(1);

		if (sht_dat_r == 1) {
			val = (val | 0x01);  //read bit
		}
		sht_clk_0;
		__delayus(1);
	}
	b = val;

	//MCU拉低data,告诉HT11已经收到了一个字节
	{
		sht_dat_0;      //MCU告诉传感器已经收到了一个字节
		__delayus(1);
		sht_clk_1;
		__delayus(1);
		sht_clk_0;
		__delayus(1);
		sht_dat_1;
	}

	m_org_data = a;
	m_org_data = (m_org_data << 8);
	m_org_data |= b;

	return m_org_data;
}

//----------------------------------------------------------------------------------
// 返回值单位0.01摄氏度
int __readTemperature(void) {
	unsigned int i;
	int temp;

	__resetConnection();
	__writeByte(0x03);

	vTaskDelay(configTICK_RATE_HZ * 400 / 1000);
	for (i = 0; i < 500; ++i) {
		if (sht_dat_r == 0) {
			break;
		}
		vTaskDelay(configTICK_RATE_HZ/10);		
	}
//	for (i = 0; i < 6000; i++) {
//		for (j = 0; j < 6000; j++) {
//			data_state1 = sht_dat_r;
//			if (data_state1 == 0) {
//				goto readtemperature;
//			}
//		}
//	}
//
//readtemperature:

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
int __readHumidity(int temp) {
	unsigned int i;
	int humi;

// 	float rh_lin;                     // rh_lin:  Humidity linear
// 	float rh_true;                     // rh_lin:  Humidity linear

	__resetConnection();
	__writeByte(0x05);

	vTaskDelay(configTICK_RATE_HZ * 400 / 1000);
	for (i = 0; i < 500; ++i) {
		if (sht_dat_r == 0) {
			break;
		}
		vTaskDelay(configTICK_RATE_HZ/10);		
	}
//	for (i = 0; i < 6000; i++) {
//		for (j = 0; j < 6000; j++) {
//			data_state1 = sht_dat_r;
//			if (data_state1 == 0) {
//				goto readhumidity;
//			}
//		}
//	}
//
//readhumidity:

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

//----------------------------------------------------------------------------------
unsigned char __softReset(void) {
	unsigned char error = 0;
	__resetConnection();              //reset communication
	__startTransfer();
	error += __writeByte(0x1e);      //send RESET-command to sensor
	return error;                     //error=1 in case of no response form the sensor
}

//SHT^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//------------------------------------------------------------------------------
int SHT11ReadTemperatureHumidity(int *temp, int *humi) {
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

	return 1;
}
