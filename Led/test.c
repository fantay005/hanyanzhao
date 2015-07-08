#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "rtc.h"
#include "second_datetime.h"
#include "math.h"
#include "time.h"
#include "norflash.h"
#include "protocol.h"
#include "gsm.h"

#define SHT_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE + 1024 * 5)

#define DetectionTime  1

#define M_PI 3.14
#define RAD  (180.0 * 3600 / M_PI)

static double richu;
static double midDayTime;
static double dawnTime;
static double jd;
static double wd;
static unsigned char sunup[3] = {0};
static unsigned char sunset[3] = {0};
static unsigned char daybreak[3] = {0};
static unsigned char daydark[3] = {0};

unsigned char *setTime_back(void){
	return sunset;
}

unsigned char *upTime_back(void){
	return sunup;
}

/*************************
     * 儒略日的计算
     *
     * @param y 年
     *
     * @param M 月
     *
     * @param d 日
     *
     * @param h 时
     *
     * @param m 分
     *
     * @param s 秒
     *
     * @return int
***************************/
static double timeToDouble(int y, int M, double d){
//  double A=0;
		double B=0;
		double jd=0;

		/*设y为给定年份，M为月份，D为改约日期*/
		/*若M > 2， Y和M不变，若M = 1或2，以y - 1代y，以M + 12代M，换句话说，如果日期在1月或2月
			则被看做是在前一年的13月或14月*/
		/*对格里高利历有：A = INT(Y/100)   B = 2 - A + INT(A/4)*/
		/*对儒略历，取B = 0*/
		/*JD = INT(365.25(Y+4716))+INT(30.6001(M+1))+D+B-1524.5 (7.1)*/
		B=-13;
		jd=floor(365.25 * (y + 4716))+ floor(30.60001 * (M + 1)) +B+ d- 1524.5;
		return jd;
}

static void doubleToTime(double time, unsigned char p[3]){
	 double t;
	 unsigned char h,m,s;

		t = time + 0.5;
		t = (t - (int) t) * 24;
		h = (unsigned char) t;
		t = (t - h) * 60;
		m = (unsigned char) t;
		t = (t - m) * 60;
		s = (unsigned char) t;
	  p[0] = h;
		p[1]  = m;
		p[2]  = s;
}

/****************************
     * @param t 儒略世纪数
     *
     * @return 太阳黄经
*****************************/
static double sunHJ(double t){
		double j;
		t = t + (32.0 * (t + 1.8) * (t + 1.8) - 20) / 86400.0 / 36525.0;
		/*儒略世纪年数，力学时*/
		j = 48950621.66 + 6283319653.318 * t + 53 * t * t - 994 + 334166 *cos(4.669257 + 628.307585 * t) + 3489 * cos(4.6261 + 1256.61517 * t) + 2060.6 * cos(2.67823 + 628.307585 * t) * t;
		return (j / 10000000);
}

static double mod(double num1, double num2){
		num2 = fabs(num2);
		/*只是取决于Num1的符号*/
		return num1 >= 0 ?(num1 - (floor(num1 / num2)) * num2 ): ((floor(fabs(num1) / num2)) * num2 - fabs(num1));
}
/********************************
     * 保证角度ε (-π, π)
     *
     * @param ag
     * @return ag
***********************************/
static double degree(double ag){
		ag = mod(ag, 2 * M_PI);
		if(ag<=-M_PI){
				ag=ag+2*M_PI;
		}
		else if(ag>M_PI){
				ag=ag-2*M_PI;
		}
		return ag;
}

/***********************************
     *
     * @param date  儒略日平午
     *
     * @param lo    地理经度
     *
     * @param la    地理纬度
     *
     * @param tz    时区
     *
     * @return 太阳升起时间
*************************************/
double sunRiseTime(double date, double lo, double la, double tz) {
		double t,j,sinJ,cosJ,gst,E,a,D,cosH0,cosH1,H0,H1,H;
		date = date - tz;
		/*太阳黄经以及它的正余弦值*/
		t = date / 36525;
		j = sunHJ(t);
		/*太阳黄经以及它的正余弦值*/
		sinJ = sin(j);
		cosJ = cos(j);
		/*其中2*M_PI*(0.7790572732640 + 1.00273781191135448*jd)恒星时（子午圈位置）*/
		gst = 2 * M_PI * (0.779057273264 + 1.00273781191135 * date) + (0.014506 + 4612.15739966 * t + 1.39667721 * t * t) / RAD;
		E = (84381.406 - 46.836769 * t) / RAD; /*黄赤交角*/
		a = atan2(sinJ * cos(E), cosJ);/*太阳赤经*/
		D = asin(sin(E) * sinJ); /*太阳赤纬*/
		cosH0 = (sin(-50 * 60 / RAD) - sin(la) * sin(D)) / (cos(la) * cos(D)); /*日出的时角计算，地平线下50分*/
		cosH1 = (sin(-6 * 3600 / RAD) - sin(la) * sin(D)) / (cos(la) * cos(D)); /*天亮的时角计算，地平线下6度，若为航海请改为地平线下12度*/
		/*严格应当区分极昼极夜，本程序不算*/
		if (cosH0 >= 1 || cosH0 <= -1){
				return -0.5;/*极昼*/
		}
		H0 = -acos(cosH0); /*升点时角（日出）若去掉负号，就是降点时角，也可以利用中天和升点计算*/
		H1 = -acos(cosH1);
		H = gst - lo - a; /*太阳时角*/
		midDayTime = date - degree(H) / M_PI / 2 + tz; /*中天时间*/
		dawnTime = date - degree(H - H1) / M_PI / 2 + tz;/*天亮时间*/
		return date - degree(H - H0) / M_PI / 2 + tz; /*日出时间，函数返回值*/
}

static void __TimeTask(void *nouse) {
	DateTime dateTime;
	uint32_t second;
	unsigned int i, len;
	short OnTime, OffTime, NowTime;
	unsigned char msg[8], buf[3];
	GatewayParam1 g;
	double jd_degrees = 117;
  double jd_seconds = 17;
  double wd_degrees = 31;
  double wd_seconds = 52;
	static unsigned char FLAG = 0, Anti = 0, Stagtege = 0, coch;
	GatewayParam2 k;
	char AfterOn = 0, AfterOff = 0; 
	portTickType lastT = 0, curT;
	uint16_t Pin_array[] = {PIN_CTRL_1, PIN_CTRL_2, PIN_CTRL_3, PIN_CTRL_4, PIN_CTRL_5, PIN_CTRL_6, PIN_CTRL_7, PIN_CTRL_8};
	GPIO_TypeDef *Gpio_array[] ={GPIO_CTRL_1, GPIO_CTRL_2, GPIO_CTRL_3, GPIO_CTRL_4, GPIO_CTRL_5, GPIO_CTRL_6, GPIO_CTRL_7, GPIO_CTRL_8};
	uint32_t BaseSecond = (7 * (365 + 365 + 365 + 366) + 2 * 365) * 24 * 60 * 60;
	unsigned short tmp[1465];
		 
	while (1) {
		 if (!RtcWaitForSecondInterruptOccured(portMAX_DELAY)) {
			continue;
		 }	 
		 
		 if(Anti == 0){
			 NorFlashRead(NORFLASH_MANAGEM_BASE, (short * )&g, (sizeof(GatewayParam1) + 1) / 2);
			 if(g.EmbedInformation == 1){
				 sscanf((const char *)&(g.Longitude), "%*1s%3s", msg);
				 jd_degrees = atoi((const char *)msg);
				 sscanf((const char *)&(g.Longitude), "%*4s%2s", msg);
				 jd_seconds = atoi((const char *)msg);

				 sscanf((const char *)&(g.Latitude), "%*1s%3s", msg);
				 wd_degrees = atoi((const char *)msg);
				 sscanf((const char *)&(g.Latitude), "%*4s%2s", msg);
				 wd_seconds = atoi((const char *)msg);
				 Anti = 1;
			 }
		 }
		 
		 if(Stagtege == 0){
			NorFlashRead(NORFLASH_MANAGEM_TIMEOFFSET, (short *)&k, (sizeof(GatewayParam2) + 1) / 2);

			 if(k.SetFlag == 1){	
				 sscanf((const char *)k.OpenOffsetTime2, "%2s", buf);
				 coch = strtol((const char *)buf, NULL, 16);
				 
				 if(coch & 0x80){
					 AfterOn = -(coch & 0x7F);
				 } else {
					 AfterOn = coch & 0x7F;
				 }
				 
				 sscanf((const char *)k.CloseOffsetTime2, "%2s", buf);
				 coch = strtol((const char *)buf, NULL, 16);
				 
				 if(coch & 0x80){					 
					 AfterOff = -(coch & 0x7F);
				 } else {
					 AfterOff = coch & 0x7F;		
				 }		
				Stagtege = 1;	
			 }
		 }
		 
		 second = RtcGetTime();
		 SecondToDateTime(&dateTime, second);
		 
		 if((dateTime.year < 0x0F) || (dateTime.month > 0x0C) || (dateTime.date > 0x1F) || (dateTime.hour > 0x3D)) {
			 continue;
		 }
		 
		 curT = xTaskGetTickCount();
		 
		 NowTime = dateTime.hour * 60 + dateTime.minute;
		 if(FLAG == 1){
			 if(AfterOff & 0x80){
				 OffTime = sunup[0] * 60 + sunup[1] + AfterOff - 256;
			 } else {
				 OffTime = sunup[0] * 60 + sunup[1] + AfterOff;
			 }
			 
			 if(AfterOn & 0x80){
				 OnTime = sunset[0] * 60 + sunset[1] + AfterOn - 256;
			 } else {
				 OnTime = sunset[0] * 60 + sunset[1] + AfterOn;
			 }
			 FLAG = 2;
		 } else if((FLAG == 2) && (Stagtege == 1)) {
			 if(AfterOff & 0x80){
				 OffTime = sunup[0] * 60 + sunup[1] + AfterOff - 256;
			 } else {
				 OffTime = sunup[0] * 60 + sunup[1] + AfterOff;
			 }
			 
			 if(AfterOn & 0x80){
				 OnTime = sunset[0] * 60 + sunset[1] + AfterOn - 256;
			 } else {
				 OnTime = sunset[0] * 60 + sunset[1] + AfterOn;
			 }
			 FLAG = 3;
			 Stagtege = 2;
		 }
		 
		 len = __OffsetNumbOfDay(&dateTime) - 1;
//		 printf("%d.\r\n", dateTime.year);
		 if ((FLAG == 0) && (dateTime.second != 0x00)){
			  DateTime OnOffLight;
			
				jd = -(jd_degrees + jd_seconds / 60) / 180 * M_PI;
				wd = (wd_degrees + wd_seconds / 60) / 180 * M_PI;
				richu = timeToDouble(dateTime.year + 2000, dateTime.month, (double)dateTime.date) - 2451544.5;
				for (i = 0; i < 10; i++){
					richu = sunRiseTime(richu, jd, wd, 8/24.0);/*逐步逼近算法10次*/
				}
				doubleToTime(richu, sunup);
				if((sunup[0] > 8) && (sunup[0] < 3)){
					continue;
				}
				
				if(sunup[1] > 59){
					continue;
				}
				
				if(sunup[2] > 59){
					continue;
				}
				
				doubleToTime(midDayTime + midDayTime - richu, sunset);
				
				if((sunset[0] > 21) && (sunset[0] < 17)){
					continue;
				}
				
				if(sunset[1] > 59){
					continue;
				}
				
				if(sunset[2] > 59){
					continue;
				}
				doubleToTime(dawnTime, daydark);
				doubleToTime(midDayTime + midDayTime - dawnTime, daybreak);	
				
				FLAG = 1;
				
				i = __OffsetNumbOfDay(&dateTime) - 1;
				
				if((dateTime.year % 2) == 0){
					NorFlashRead(NORFLASH_ONOFFTIME2, (short *)tmp, 4 * i);
					if(tmp[4 * i - 4] != 0xFFFF){
						continue;
					}
				} else {
					NorFlashRead(NORFLASH_ONOFFTIME1, (short *)tmp, 4 * i);
					if(tmp[4 * i - 4] != 0xFFFF){
						continue;
					}					
				}
				
				OnOffLight.year = dateTime.year;
				OnOffLight.month = dateTime.month;
				OnOffLight.date = dateTime.date;			
			
				if((dateTime.year % 2) == 0){
					NorFlashRead(NORFLASH_ONOFFTIME2, (short *)tmp, 4 * i);
					OnOffLight.hour = sunup[0];
					OnOffLight.minute = sunup[1];
					OnOffLight.second = sunup[2];
					
					tmp[i * 4 + 2] = (DateTimeToSecond(&OnOffLight) + BaseSecond) >> 16 ;
					tmp[i * 4 + 3] = (DateTimeToSecond(&OnOffLight) + BaseSecond) & 0xFFFF;
					
					OnOffLight.hour = sunset[0];
					OnOffLight.minute = sunset[1];
					OnOffLight.second = sunset[2];
				
					tmp[i * 4] = (DateTimeToSecond(&OnOffLight) + BaseSecond) >> 16 ;
					tmp[i * 4 + 1] = (DateTimeToSecond(&OnOffLight) + BaseSecond) & 0xFFFF;
					
					NorFlashWrite(NORFLASH_ONOFFTIME2, (short *)tmp, i * 4 + 4);
					
				} else {
					NorFlashRead(NORFLASH_ONOFFTIME1, (short *)tmp, 4 * i);
					
					OnOffLight.hour = sunup[0];
					OnOffLight.minute = sunup[1];
					OnOffLight.second = sunup[2];
					
					tmp[i * 4 + 2] = (DateTimeToSecond(&OnOffLight) + BaseSecond) >> 16 ;
					tmp[i * 4 + 3] = (DateTimeToSecond(&OnOffLight) + BaseSecond) & 0xFFFF;
					
					OnOffLight.hour = sunset[0];
					OnOffLight.minute = sunset[1];
					OnOffLight.second = sunset[2];
					
					tmp[i * 4] = (DateTimeToSecond(&OnOffLight) + BaseSecond) >> 16 ;
					tmp[i * 4 + 1] = (DateTimeToSecond(&OnOffLight) + BaseSecond) & 0xFFFF;
					
					NorFlashWrite(NORFLASH_ONOFFTIME1, (short *)tmp, i * 4 + 4);	
					
				}
						
				
		} else if ((FLAG >= 2) && (dateTime.hour == (OffTime / 60)) && (dateTime.minute == (OffTime % 60)) && (dateTime.second == sunup[2])) {
		
			if(GPIO_ReadInputDataBit(Gpio_array[0], Pin_array[0]) == 1){
				if(dateTime.year % 2){
					NorFlashRead(NORFLASH_ONOFFTIME1, (short *)tmp, len * 4);
					tmp[len * 4 - 2] = (second + BaseSecond) >> 16;
					tmp[len * 4 - 1] = (second + BaseSecond) & 0xFFFF;
					NorFlashWrite(NORFLASH_ONOFFTIME1, (short *)tmp, len * 4);
				} else {
					NorFlashRead(NORFLASH_ONOFFTIME2, (short *)tmp, len * 4);
					tmp[len * 4 - 2] = (second + BaseSecond) >> 16;
					tmp[len * 4 - 1] = (second + BaseSecond) & 0xFFFF;
					NorFlashWrite(NORFLASH_ONOFFTIME2, (short *)tmp, len * 4);
				}
			}
  		GPIO_SetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
			for(i = 0; i < 8; i++){
				GPIO_ResetBits(Gpio_array[i], Pin_array[i]);			
			}		
			GPIO_ResetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
			
		} else if ((FLAG >= 2) && (dateTime.hour == (OnTime / 60)) && (dateTime.minute == (OnTime % 60)) && (dateTime.second == sunset[2])) {
		
		  if(GPIO_ReadInputDataBit(Gpio_array[0], Pin_array[0]) == 0){
				if(dateTime.year % 2){
					NorFlashRead(NORFLASH_ONOFFTIME1, (short *)tmp, len * 4);
					tmp[len * 4 - 4] = (second + BaseSecond) >> 16;
					tmp[len * 4 - 3] = (second + BaseSecond) & 0xFFFF;
					NorFlashWrite(NORFLASH_ONOFFTIME1, (short *)tmp, len * 4);
				} else {
					NorFlashRead(NORFLASH_ONOFFTIME2, (short *)tmp, len * 4);
					tmp[len * 4 - 4] = (second + BaseSecond) >> 16;
					tmp[len * 4 - 3] = (second + BaseSecond) & 0xFFFF;
					NorFlashWrite(NORFLASH_ONOFFTIME2, (short *)tmp, len * 4);
				}				
			}
			GPIO_SetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
			for(i = 0; i < 8; i++){
				GPIO_SetBits(Gpio_array[i], Pin_array[i]);				
			}
			GPIO_ResetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
			
		} else if ((dateTime.hour == 0x00) && (dateTime.minute == 0x01) && (dateTime.second == 0x00)) {
			
			FLAG = 0;
			
		} else if((dateTime.hour == 0x0C)&& (dateTime.minute == 0x00) && (dateTime.second == 0x00)){
			
			NVIC_SystemReset();
			
		} else if((NowTime > OffTime) && (NowTime < OnTime)){
#if defined (__MODEL_DEBUG__)		
		
#else			
			if(lastT > curT){
				lastT = 0;
			}
			if((curT - lastT) > configMINIMAL_STACK_SIZE * DetectionTime){
				GPIO_SetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
				for(i = 0; i < 8; i++){
					GPIO_ResetBits(Gpio_array[i], Pin_array[i]);
				}		
				GPIO_ResetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
				lastT = curT;
			}
#endif			
		} else if((NowTime > OnTime) || (NowTime < OffTime)){
#if defined (__MODEL_DEBUG__)			
			
#else			
			if(lastT > curT){
				lastT = 0;
			}
			if((curT - lastT) > configMINIMAL_STACK_SIZE * DetectionTime){
				GPIO_SetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
				for(i = 0; i < 8; i++){
					GPIO_SetBits(Gpio_array[i], Pin_array[i]);			
				}		
				GPIO_ResetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
				lastT = curT;
			}
#endif			
		} 
	}
}

void TimePlanInit(void) {
	xTaskCreate(__TimeTask, (signed portCHAR *) "TEST", SHT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL);
}


