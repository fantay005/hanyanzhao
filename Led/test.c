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

#define SHT_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE + 128 )

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

static void __ledTestTask(void *nouse) {
	DateTime dateTime;
	DateTime OnOffLight;
	uint32_t second;	
	unsigned int i;
	short tmp[732];
	unsigned char msg[8];
	GatewayParam1 g;
	double jd_degrees;
  double jd_seconds;
  double wd_degrees;
  double wd_seconds;
	static unsigned char FLAG = 0;
	
	NorFlashRead(NORFLASH_MANAGEM_BASE, (short * )&g, (sizeof(GatewayParam1) + 1) / 2);
	sscanf((const char *)&(g.Longitude), "%*1s%3s", msg);
	jd_degrees = atoi((const char *)msg);
	sscanf((const char *)&(g.Longitude), "%*4s%6s", msg);
	jd_seconds = atoi((const char *)msg);
	
	sscanf((const char *)&(g.Latitude), "%*1s%3s", msg);
	wd_degrees = atoi((const char *)msg);
	sscanf((const char *)&(g.Latitude), "%*4s%6s", msg);
	wd_seconds = atoi((const char *)msg);
	while (1) {
		 if (!RtcWaitForSecondInterruptOccured(portMAX_DELAY)) {
			continue;
		 }

		 second = RtcGetTime();
		 SecondToDateTime(&dateTime, second);
//		 printf("%d.\r\n", dateTime.year);
		 if ((FLAG == 0) && (dateTime.second != 0x00) && (strncmp((const char *)&(g.Success), "SUCCEED", 7) == 0)){
				jd = -(jd_degrees + jd_seconds / 60) / 180 * M_PI;
				wd = (wd_degrees + wd_seconds / 60) / 180 * M_PI;
				richu = timeToDouble(dateTime.year + 2000, dateTime.month, (double)dateTime.date) - 2451544.5;
				for (i = 0; i < 10; i++){
					richu = sunRiseTime(richu, jd, wd, 8/24.0);/*逐步逼近算法10次*/
				}
				doubleToTime(richu, sunup);
				doubleToTime(midDayTime + midDayTime - richu, sunset);
				doubleToTime(dawnTime, daydark);
				doubleToTime(midDayTime + midDayTime - dawnTime, daybreak);
				
				i = __OffsetNumbOfDay(&dateTime) - 1;
				
				OnOffLight.year = dateTime.year;
				OnOffLight.month = dateTime.month;
				OnOffLight.date = dateTime.date;			
				OnOffLight.hour = sunup[0];
				OnOffLight.minute = sunup[1];
				OnOffLight.second = sunup[2];
				
				DateTimeToSecond(&OnOffLight);
				
				tmp[i * 4] = DateTimeToSecond(&OnOffLight) >> 8 ;
				tmp[i * 4 + 1] = DateTimeToSecond(&OnOffLight) & 0xFF;
				
				if((dateTime.year % 2) == 0){
					
					NorFlashWrite(NORFLASH_ONOFFTIME2, tmp, i * 4 + 2);
					
					OnOffLight.hour = sunset[0];
					OnOffLight.minute = sunset[1];
					OnOffLight.second = sunset[2];
					
					DateTimeToSecond(&OnOffLight);
					
					tmp[i * 4 + 2] = DateTimeToSecond(&OnOffLight) >> 8 ;
					tmp[i * 4 + 3] = DateTimeToSecond(&OnOffLight) & 0xFF;
					
					NorFlashWrite(NORFLASH_ONOFFTIME2, tmp, i * 4 + 4);
				} else {
					
					NorFlashWrite(NORFLASH_ONOFFTIME1, tmp, i * 4 + 2);
					
					OnOffLight.hour = sunset[0];
					OnOffLight.minute = sunset[1];
					OnOffLight.second = sunset[2];
					
					DateTimeToSecond(&OnOffLight);
					
					tmp[i * 4 + 2] = DateTimeToSecond(&OnOffLight) >> 8 ;
					tmp[i * 4 + 3] = DateTimeToSecond(&OnOffLight) & 0xFF;
					
					NorFlashWrite(NORFLASH_ONOFFTIME1, tmp, i * 4 + 4);							
				}
				FLAG = 1;		
		} else if ((dateTime.hour == sunup[0]) && (dateTime.minute == sunup[1]) && (dateTime.second == sunup[2])) {
			
		} else if ((dateTime.hour == sunset[0]) && (dateTime.minute == sunset[1]) && (dateTime.second == sunset[2])) {
			
		} else if ((dateTime.hour == 0x00) && (dateTime.minute == 0x01) && (dateTime.second == 0x00)) {
			FLAG = 0;
		}
	}
}

void TimePlanInit(void) {
	xTaskCreate(__ledTestTask, (signed portCHAR *) "TST", SHT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
}


