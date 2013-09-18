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
#include "stm32f10x_tim.h"
#include "stm32f10x_rtc.h"

#define CLK_GPIO_PORT GPIOB
#define CLK_GPIO_PIN  GPIO_Pin_12

#define DAT_GPIO_PORT GPIOB
#define DAT_GPIO_PIN  GPIO_Pin_13

#define DATE_GPIO_PORT GPIOE
#define DATA_GPIO_PIN  GPIO_Pin_2

#define WTH_GPIO_PORT  GPIOF
#define WTH_GPIO_PIN   GPIO_Pin_7

#define CLK245_GPIO_PORT GPIOF
#define CLK245_GPIO_PIN  GPIO_Pin_8

#define LANCH_GPIO_PORT  GPIOE
#define LANCH_GPIO_PIN   GPIO_Pin_6

struct timer {
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char week;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
} time_1, time_2;

//土壤温湿度传感器也是SHT10
#define sht_clk_0  GPIO_ResetBits(CLK_GPIO_PORT,CLK_GPIO_PIN)
#define sht_clk_1  GPIO_SetBits(CLK_GPIO_PORT, CLK_GPIO_PIN)
#define sht_dat_0  GPIO_ResetBits(DAT_GPIO_PORT, DAT_GPIO_PIN)
#define sht_dat_1  GPIO_SetBits(DAT_GPIO_PORT ,DAT_GPIO_PIN)
#define sht_dat_r  GPIO_ReadInputDataBit(DAT_GPIO_PORT, DAT_GPIO_PIN) //读出数据线数据

//用于显示T_T_H的I/O口--------------------------------
#define tht_dat1_1   GPIO_SetBits(DATE_GPIO_PORT, DATA_GPIO_PIN)   //PE.2  年月日星期
#define tht_dat1_0   GPIO_ResetBits(DATE_GPIO_PORT, DATA_GPIO_PIN)

#define tht_dat2_1   GPIO_SetBits(WTH_GPIO_PORT, WTH_GPIO_PIN)   //PF.7  报警时分温度湿度
#define tht_dat2_0   GPIO_ResetBits(WTH_GPIO_PORT, WTH_GPIO_PIN)

#define tht_clk_1    GPIO_SetBits(CLK245_GPIO_PORT, CLK245_GPIO_PIN)   //PF.8
#define tht_clk_0    GPIO_ResetBits(CLK245_GPIO_PORT, CLK245_GPIO_PIN)

#define tht_lanch_1  GPIO_SetBits(LANCH_GPIO_PORT, LANCH_GPIO_PIN)   //PE.6
#define tht_lanch_0  GPIO_ResetBits(LANCH_GPIO_PORT, LANCH_GPIO_PIN)

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

	sht_clk_0;
	sht_dat_0;

	GPIO_InitStructure.GPIO_Pin = CLK_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(DATE_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = WTH_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(WTH_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = CLK245_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(CLK245_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = LANCH_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(LANCH_GPIO_PORT, &GPIO_InitStructure);


}


void __delayus(unsigned int t) {
	unsigned int i;
	for (i = 0; i < t; ++i);
}

void __startTransfer(void) {
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

char __writeByte(unsigned char value_w) {
	unsigned char i, error = 0;

	sht_dat_1;
	__delayus(1);

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

	return error;   //error=1 in case of no acknowledge

}

int __readData(void)

{
	unsigned char i, val = 0;
	unsigned char a, b;
	int m_org_data;

	sht_dat_1;
	sht_clk_0;
	val = 0;

	__delayus(1);

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

	{
		sht_dat_0;      //MCU告诉传感器已经收到了一个字节
		__delayus(1);
		sht_clk_1;
		__delayus(3);
		sht_clk_0;
		__delayus(1);
		sht_dat_1;
	}

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

	vTaskDelay(configTICK_RATE_HZ * 320 / 1000);
	for (i = 0; i < 500; ++i) {
		if (sht_dat_r == 0) {
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
int __readHumidity(int temp) {
	unsigned int i;
	int humi;

	__resetConnection();
	__writeByte(0x05);

	vTaskDelay(configTICK_RATE_HZ * 400 / 1000);
	for (i = 0; i < 500; ++i) {
		if (sht_dat_r == 0) {
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
int SHT10ReadTemperatureHumidity(int *temp, int *humi) {
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

//------------------------------------------------
unsigned char format_change(unsigned char digit) {
	unsigned char t = 0;
	switch (digit) {
	case 0x00:
		t = 0xBF;
		break;
	case 0x01:
		t = 0x0B;
		break;
	case 0x02:
		t = 0x77;
		break;
	case 0x03:
		t = 0x5F;
		break;
	case 0x04:
		t = 0xCB;
		break;
	case 0x05:
		t = 0xDD;
		break;
	case 0x06:
		t = 0xFD;
		break;
	case 0x07:
		t = 0x0F;
		break;
	case 0x08:
		t = 0xFF;
		break;
	case 0x09:
		t = 0xDF;
		break;
	case 0x0a:
		t = 0x01;
		break;
	case 0x0b:
		t = 0x41;
		break;
	default :
		t = 0x00;
	}
	return 	t;
}

void display_tht(unsigned char disp_flash) {
	unsigned char i, j, temp0, temp1;
	unsigned char disp_str1[10];
	unsigned char disp_str2[10];
	int temperature, humidity;

	disp_str1[0] = format_change(time_2.year / 10);  //year3... year4
	disp_str1[1] = format_change(time_2.year % 10);
	disp_str1[2] = format_change((time_2.month) / 10); //mounth1, mounth2
	disp_str1[3] = format_change((time_2.month) % 10);
	disp_str1[4] = format_change((time_2.day) / 10);   //day1
	disp_str1[5] = format_change((time_2.day) % 10); //day2

	disp_str1[6] = format_change((time_2.hour) / 10);  //时
	disp_str1[7] = format_change((time_2.hour) % 10);
	disp_str1[8] = format_change((time_2.minute) / 10); //分
	disp_str1[9] = format_change((time_2.minute) % 10);

	if ((time_2.week == 0) || (time_2.week > 8)) {
		time_2.week = 8;
	}

	disp_str2[9] = format_change((time_2.week) & (0x0f)); //week

	if (disp_flash < 10) {
		disp_str1[disp_flash] = (disp_str1[disp_flash] & 0x01);   //年,月,日闪烁
	}

	disp_str2[9] = format_change((time_2.week) & (0x0f)); //week
	if (time_2.week == 7) {
		disp_str2[9] = format_change(0x08);            //Sunday
	} else {
		disp_str2[9] = format_change((time_2.week) & (0x0f)); //week
	}

	temp0 = (((unsigned char)temperature) / 10); //显示温度
	temp1 = (((unsigned char)temperature) % 10);
	disp_str2[8] = format_change(temp0);
	disp_str2[7] = format_change(temp1);

	temp0 = (((unsigned char)humidity) / 10); //显示湿度
	temp1 = (((unsigned char)humidity) % 10);
	disp_str2[6] = format_change(temp0);
	disp_str2[5] = format_change(temp1);

	TIM_Cmd(TIM2, DISABLE);
	TIM_Cmd(TIM3, DISABLE);

	tht_dat1_1;
	tht_dat2_1;
	tht_clk_1;
	tht_lanch_1;
	__delayus(8);
	for (i = 0; i < 10; i++) {
		for (j = 0; j < 8; j++) {
			if (((disp_str1[i] >> j) & 0x01) == 0) {
				tht_dat1_1;
			} else {
				tht_dat1_0;
			}
			if (((disp_str2[i] >> j) & 0x01) == 0) {
				tht_dat2_1;
			} else {
				tht_dat2_0;
			}

			__delayus(8);
			tht_clk_0;
			__delayus(8);
			tht_clk_1;
			__delayus(8);
		}
	}
	tht_lanch_0;
	__delayus(8);
	tht_lanch_1;
	__delayus(8);
	TIM_Cmd(TIM2, ENABLE);
	TIM_Cmd(TIM3, ENABLE);
}

//void read_dis_time_sht(void) {
//	int temperature, humidity;
//
//	temperature = __readTemperature(); //测量温度和湿度(float)
//	humidity = __readHumidity();    //测量温度和湿度(float)
//
//	if (((temperature > -9) && (temperature < 59))
//			&& ((humidity > 0.1) && (humidity < 99))) {
//			__delayus(1);
//		
//	}
//
//	Time_Display(RTC_GetCounter());
//
//	display_tht(11);
//}


