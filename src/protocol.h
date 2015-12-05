#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "gsm.h"
#include "stm32f10x_gpio.h"

#define __HEXADDRESS__   1

typedef struct {
	unsigned char header;
	unsigned char addr[10];
	unsigned char contr[2];
	unsigned char lenth[2];
} ProtocolHead;

typedef struct {
	unsigned char FH;
	unsigned char AD[4];
	unsigned char CT[2];
	unsigned char LT[2];
} FrameHeader;

typedef struct{
	unsigned char GatewayID[6];          /*网关身份标识*/
	unsigned char Longitude[10];         /*经度*/
	unsigned char Latitude[10];          /*纬度*/
	unsigned char FrequPoint;            /*ZIGBEE频点*/
	unsigned char IntervalTime[2];       /*自动上传数据时间间隔*/
	unsigned char NumbOfLoop[2];         /*回路数*/
	unsigned char EmbedInformation;      /*信息置入标识*/
}GatewayParam1;                        /*网关参数下载帧1*/

typedef struct{
	unsigned char OpenOffsetTime1[2];    /*开灯偏移时间1*/
	unsigned char OpenOffsetTime2[2];    /*开灯偏移时间1*/
	unsigned char CloseOffsetTime1[2];   /*关灯偏移时间1*/
	unsigned char CloseOffsetTime2[2];   /*关灯偏移时间2*/
	unsigned char SetFlag;
}GatewayParam2;                        /*网关参数下载帧1*/ 

typedef struct{
	unsigned char HVolLimitValL1[4];      /*总回路L1/L2/L3高电压限定值*/
	unsigned char HVolLimitValL2[4]; 
	unsigned char HVolLimitValL3[4]; 
	unsigned char LVolLimitValL1[4];      /*总回路L1/L2/L3低电压限定值*/ 
	unsigned char LVolLimitValL2[4]; 
	unsigned char LVolLimitValL3[4]; 
	unsigned char NoloadCurLimitValL1[4]; /*总回路L1/L2/L3/N相空载电流限定值*/
	unsigned char NoloadCurLimitValL2[4];
	unsigned char NoloadCurLimitValL3[4];
	unsigned char NoloadCurLimitValN[4];
	unsigned char PhaseCurLimitValL1[4];  /*总回路A/B/C/N相电流限定值*/
	unsigned char PhaseCurLimitValL2[4];
	unsigned char PhaseCurLimitValL3[4];
	unsigned char PhaseCurLimitValN[4];
	unsigned char NumbOfCNBL;            /*连续不亮灯数量*/
	unsigned char OtherWarn[2];          /*其他警告*/ 
	unsigned char SetWarnFlag;
}GatewayParam3;

typedef struct{
	unsigned char AddrOfZigbee[4];   /*ZIGBEE地址*/
	unsigned char NorminalPower[4];  /*标称功率*/ 
	unsigned char Loop;              /*所属回路*/ 
	unsigned char LightPole[4];      /*所属灯杆号*/ 
	unsigned char LightSourceType;   /*光源类型*/ 
	unsigned char LoadPhaseLine;     /*负载相线*/ 
	unsigned char Attribute[2];      /*主/辅/投属性*/ 
	unsigned char TimeOfSYNC[12];    /*灯参数同步时间*/
	unsigned char Empty;
	unsigned char CommState;         /*通信状态*/
	unsigned short InputPower;       /*输入功率*/
	unsigned int UpdataTime;    /*上传数据时间*/
}Lightparam;


typedef struct{
	unsigned char AddrOfZigb[4];    /*Zigbee地址*/
//	unsigned char Cache;						/*一个缓冲数据*/
	unsigned char SchemeType[2];   /*方案类型*/
	unsigned char DimmingNOS;      /*调光段数*/
	unsigned char FirstDCTime[4];  /*第一段调光持续时间*/
	unsigned char FirstDPVal[2];   /*第一段调光功率值*/
	unsigned char SecondDCTime[4]; /*第二段调光持续时间*/
	unsigned char SecondDPVal[2];  /*第二段调光功率值*/
	unsigned char ThirdDCTime[4];  /*第三段调光持续时间*/
	unsigned char ThirdDPVal[2];   /*第三段调光功率值*/
	unsigned char FourthDCTime[4]; /*第四段调光持续时间*/
	unsigned char FourthDPVal[2];  /*第四段调光功率值*/
	unsigned char FifthDCTime[4];  /*第五段调光持续时间*/
	unsigned char FifthDPVal[2];   /*第五段调光功率值*/
	unsigned char SYNCTINE[12];    /*策略同步标识*/
}StrategyParam;


void ProtocolHandler(ProtocolHead *head, char *p);
unsigned char *ProtocolRespond(unsigned char address[10], unsigned char  type[2], const char *msg, unsigned char *size);

#endif
