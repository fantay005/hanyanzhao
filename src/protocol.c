#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gsm.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "protocol.h"
#include "xfs.h"
#include "misc.h"
#include "sms.h"
#include "stm32f10x_gpio.h"
#include "norflash.h"
#include "zklib.h"
#include "libupdater.h"
#include "norflash.h"
#include "sms_cmd.h"

typedef enum{
	ACKERROR = 0,           /*从站应答异常*/
	GATEPARAM = 0x01,       /*网关参数下载*/
	LIGHTPARAM = 0x02,      /*灯参数下载*/
	STRATEGY = 0x03,        /*策略下载*/
	DIMMING = 0x04,         /*灯调光控制*/
	LAMPSWITCH = 0x05,      /*灯开关控制*/
	READDATA = 0x06,        /*读镇流器数据*/
	LOOPCONTROL = 0x07,     /*网关回路控制*/
	DATAQUERY = 0x08,       /*网关数据查询*/
	TIMEQUERY = 0x09,       /*网关开关灯时间查询*/
	AUTOWORK = 0x0A,        /*灯自主运行*/
	TIMEADJUST = 0x0B,      /*校时*/
	VERSIONQUERY = 0x0C,    /*网关软件版本号查询*/ 
  ELECTRICGATHER = 0x0E,  /*电量采集软件版本号查询*/	
	LUXUPLOAD = 0x0F,       /*光照度上传*/
	SMSSEND = 0x10,         /*短信发送*/
	ADDRESSQUERY = 0x11,    /*网关地址查询*/
	SETSERVERIP = 0x14,     /*设置网关目标服务器IP*/
	GATEUPGRADE = 0x15,     /*网关远程升级*/
	GATHERUPGRADE = 0x1E,   /*电量采集模块远程升级*/
	BALLASTUPGRADE= 0x2A,   /*镇流器远程升级*/
	RESTART = 0x3F,         /*设备复位*/
	RETAIN,          /*保留*/
} GatewayType;

typedef struct {
	unsigned char header;
	unsigned char addr[10];
	unsigned char contr[2];
	unsigned char lenthH;
	unsigned char lenthL;
} ProtocolHead;

typedef struct {
	unsigned char BCC[2];
	unsigned char x01;
} ProtocolTail;

typedef struct {
	unsigned char Teleph[4];
	unsigned char AreaID[2];
	unsigned char GateID[4];
} AddrField;

char *ProtocolSend(AddrField *address, GatewayType type, const char *msg, int *size) {
	int i;
	unsigned int verify = 0;
	unsigned char *p, *ret;
	unsigned char hexTable[] = "0123456789ABCDEF";
	int len = (msg == NULL ? 0 : strlen(msg));
	*size = sizeof(ProtocolHead) + len + sizeof(ProtocolTail);
	ret = pvPortMalloc(*size);
	{
		ProtocolHead *h = (ProtocolHead *)ret;
		h->header = 0x02;	
		h->addr[0] = address->Teleph[0];
		h->addr[1] = address->Teleph[1];
		h->addr[2] = address->Teleph[2];
		h->addr[3] = address->Teleph[3];
		h->addr[4] = address->AreaID[0];
		h->addr[5] = address->AreaID[1];
		h->addr[6] = address->GateID[0];
		h->addr[7] = address->GateID[1];
		h->addr[8] = address->GateID[2];
		h->addr[9] = address->GateID[3];
		h->contr[0] = hexTable[(type >> 4) & 0x0F];
		h->contr[1] = hexTable[type & 0x0F];
		h->lenthH = hexTable[(len >> 4) & 0x0F];
		h->lenthL = hexTable[len & 0x0F];
	}
	
	if (msg != NULL) {
		strcpy((char *)(ret + sizeof(ProtocolHead)), msg);
	}
	
	p = ret;
	for (i = 0; i < len + sizeof(ProtocolHead); ++i) {
		verify ^= *p++;
	}
	
	*p++ = hexTable[(verify >> 4) & 0x0F];
	*p++ = hexTable[verify & 0x0F];
	*p++ = 0x03;
	return ret;
}

void ProtocolDestroyMessage(const char *p) {
	vPortFree((void *)p);
}


static void HandleGatewayParam(const char *p) {
	
}

static void HandleLightParam(const char *p) {
	
}

static void HandleStrategy(const char *p) {
	
}

static void HandleLightDimmer(const char *p) {
	
}

static void HandleLightOnOff(const char *p) {
	
}

static void HandleReadBSNData(const char *p) {
	
}

static void HandleGWloopControl(const char *p) {
	
}

static void HandleGWDataQuery(const char *p) {
	
}

static void HandleGWTurnTimeQuery(const char *p) {
	
}

static void HandleLightAuto(const char *p) {
	
}

static void HandleAdjustTime(const char *p) {
	
}

static void HandleGWVersQuery(const char *p) {
	
}

static void HandleEGVersQuery(const char *p) {
	
}

static void HandleIllumUpdata(const char *p) {
	
}

static void HandleSMSSend(const char *p) {
	
}

static void HandleGWAddrQuery(const char *p) {
	
}

static void HandleSetGWServ(const char *p) {
	
}

static void HandleGWUpgrade(const char *p) {
	
}

static void HandleEGUpgrade(const char *p) {
	
}

static void HandleBSNUpgrade(const char *p) {
	
}

typedef void (*ProtocolHandleFunction)(const char *p);
typedef struct {
	unsigned char type;
	ProtocolHandleFunction func;
} ProtocolHandleMap;


void ProtocolHandler(unsigned char tmp, char *p) {
	int i;
	const static ProtocolHandleMap map[] = {  /*GW: gateway  网关*/
		{GATEPARAM,      HandleGatewayParam},   /*EG: electric quantity gather 电量采集器*/
		{LIGHTPARAM,     HandleLightParam},     /* illuminance ： 光照度*/
		{STRATEGY,       HandleStrategy},       /*BSN: 钠灯镇流器*/
		{DIMMING,        HandleLightDimmer},
		{LAMPSWITCH,     HandleLightOnOff},
		{READDATA,       HandleReadBSNData},
		{LOOPCONTROL,    HandleGWloopControl},
		{DATAQUERY,      HandleGWDataQuery},
		{TIMEQUERY,      HandleGWTurnTimeQuery},
		{AUTOWORK,       HandleLightAuto},
		{TIMEADJUST,     HandleAdjustTime},
		{VERSIONQUERY,   HandleGWVersQuery},
		{ELECTRICGATHER, HandleEGVersQuery},
		{LUXUPLOAD,      HandleIllumUpdata},
		{SMSSEND,        HandleSMSSend},
		{ADDRESSQUERY,   HandleGWAddrQuery},
		{SETSERVERIP,    HandleSetGWServ},
		{GATEUPGRADE,    HandleGWUpgrade},
		{GATHERUPGRADE,  HandleEGUpgrade},
		{BALLASTUPGRADE, HandleBSNUpgrade},
		{NULL, NULL}
	};

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
			if ((map[i].type == tmp)) {
				map[i].func(p + sizeof(ProtocolHead));
				break;
			}
	}
}
