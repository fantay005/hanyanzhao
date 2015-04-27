#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "gsm.h"

typedef struct {
	unsigned char header;
	unsigned char addr[10];
	unsigned char contr[2];
	unsigned char lenth[2];
} ProtocolHead;

typedef struct{
	unsigned char GatewayID[6];          /*网关身份标识*/
	unsigned char Longitude[10];         /*经度*/
	unsigned char Latitude[10];          /*纬度*/
	unsigned char FrequPoint;            /*ZIGBEE频点*/
	unsigned char IntervalTime[2];       /*自动上传数据时间间隔*/
	unsigned char TransfRatio[2];        /*互感器倍数*/
}GatewayParam1;                        /*网关参数下载帧1*/

typedef struct{
	unsigned char OpenOffsetTime1[2];    /*开灯偏移时间1*/
	unsigned char OpenOffsetTime2[2];    /*开灯偏移时间1*/
	unsigned char CloseOffsetTime1[2];   /*关灯偏移时间1*/
	unsigned char CloseOffsetTime2[2];   /*关灯偏移时间2*/
}GatewayParam2;                        /*网关参数下载帧1*/ 

typedef struct{
	unsigned char HVolLimitVal[12];      /*总回路L1/L2/L3高电压限定值*/
	unsigned char LVolLimitVal[12];      /*总回路L1/L2/L3低电压限定值*/ 
	unsigned char NoloadCurLimitVal[16]; /*总回路L1/L2/L3/N相空载电流限定值*/
	unsigned char PhaseCurLimitVal[16];  /*总回路A/B/C/N相电流限定值*/
	unsigned char NumbOfCNBL;            /*连续不亮灯数量*/
	unsigned char OtherWarn[2];          /*其他警告*/ 
}GatewayParam3;

void ProtocolHandler(ProtocolHead *head, char *p);

#endif
