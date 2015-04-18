#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "protocol.h"
#include "gsm.h"
#include "xfs.h"
#include "misc.h"
#include "sms.h"
#include "led_lowlevel.h"
#include "stm32f10x_gpio.h"
#include "norflash.h"
#include "zklib.h"
#include "libupdater.h"
#include "unicode2gbk.h"
#include "led_lowlevel.h"
#include "display.h"
#include "softpwm_led.h"
#include "version.h"
#include "second_datetime.h"
#include "fm.h"
#include "soundcontrol.h"
#include "sms_cmd.h"


USERParam __userParam;

typedef struct{
	char mobilefare[10];
	char unicomfare[10];
	char mobileflow[10];
	char unicomflow[10];
	char mobilemenu[10];
	char unicommenu[10];
}Business;

Business __business = {"302","101","703","5102","77"};

static inline void __storeUSERParam(void) {
	NorFlashWrite(USER_PARAM_STORE_ADDR, (const short *)&__userParam, sizeof(__userParam));
}

static void __restorUSERParam(void) {
	NorFlashRead(USER_PARAM_STORE_ADDR, (short *)&__userParam, sizeof(__userParam));
}

static inline void __storeBUSIParam(void) {
	NorFlashWrite(BUSI_PARAM_STORE_ADDR, (const short *)&__business, sizeof(__business));
}

static void __restorBUSIParam(void) {
	NorFlashRead(BUSI_PARAM_STORE_ADDR, (short *)&__business, sizeof(__business));
}

unsigned char *USERpara(unsigned char *p){
	unsigned char len = 0, i, j, k = 0;
	__restorUSERParam();
	p = pvPortMalloc(100);
	for(i = 0; i < 6; i++){
		if((__userParam.user[i][0] == 0xff) || (__userParam.user[i][0] == 0x00)){
			k++;
			for(j = 0; j < 12; j++){
			  __userParam.user[i][j] = 0x00;
			}
		} else {
		  len += sprintf(&p[len], "%d%s.", (i + 1), __userParam.user[i]);
		}
  }
	if(k == 6){
		*p = 0;
	}	
	return p;
}

void __storeSMS1(const char *sms, int len) {
	NorFlashWrite(SMS1_PARAM_STORE_ADDR, (const short *)sms, len);
}

static inline bool __isValidUser(const char *p) {
	int i;
	if(strlen(p) < 5){
	   return false;
	}
	for (i = 0; i < 12; ++i) {
		if (p[i] == 0) {
			return true;
		}
		if (!isxdigit(p[i])) {
			return false;
		}
	}

	return false;
}

// return 1-6
// 0 no found
static int __userIndex(const char *user) {
	int i;

	if (!__isValidUser(__userParam.user[0])) {
		return 1;
	}

	for (i = 0; i < ARRAY_MEMBER_NUMBER(__userParam.user) ; ++i) {
		if (strcmp(user, __userParam.user[i]) == 0) {
			return i + 1;
		}
	}

	return 0;
}


// index  1 - 6
const char *__user(int index) {
	if (index <= 0) {
		return NULL;
	}
	if (index >= 7) {
		return NULL;
	}

	return __isValidUser(__userParam.user[index - 1]) ? __userParam.user[index - 1] : NULL;
}

// index  1 - 6
static void __setUser(int index, const char *user) {
	strcpy(__userParam.user[index - 1], user);
}



void SMSCmdSetUser(int index, const char *user) {
	if (index <= 0 || index >= 7) {
		return;
	}
	if (strlen(user) >= 12) {
		return;
	}
	__setUser(index, user);
	__storeUSERParam();
}

void SMSCmdRemoveUser(int index) {
	const char user[12] = {0};
	if (index <= 0 || index >= 7) {
		return;
	}
	__setUser(index, user);
	__storeUSERParam();
}

// index  1 - 6
static void __sendToUser(int index, const char *content, int len) {
	char *pdu = pvPortMalloc(300);
	const char *dest = __user(index);
	if (dest == NULL) {
		return;
	}
	len = SMSEncodePduUCS2(pdu, dest, content, len);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
}

static void __cmd_LOCK_Handler(const SMSInfo *p) {
	int i;
	bool isAlock;
	const char *pcontent;
	char *sms;
	int index;
	// 已将2号用户授权与13800000000
	static const char __toUser1[] = {
		0X5D, 0XF2, //已
		0x5C, 0x06, //将
		0x00, 0x00,
		0x53, 0xF7,
		0x75, 0x28,
		0x62, 0x37,
		0x63, 0x88,
		0x67, 0x43,
		0x4E, 0x0E,
	};

	static const char __toUser[] = {
		0x60, 0xA8, //您
		0x5D, 0xF2, //已
		0x88, 0xAB, //被
		0x63, 0x88, //授
		0x4E, 0x88, //予
		0x00, 0x00, //k
		0x53, 0xF7, //号
		0x75, 0x28, //用
		0x62, 0x37, //户
		0x67, 0x43, //权
		0x96, 0x50, //限
	};

	if (strncasecmp((char *)p->content, "<alock>", 7) == 0) {
		isAlock = true;
		pcontent = (const char *)&p->content[7];
	} else if (strncasecmp((char *)p->content, "<lock>", 6) == 0){
		isAlock = false;
		pcontent = (const char *)&p->content[6];
	}

	index = pcontent[0] - '0';
	if (index > 6) {
		return;
	}
	if (index < 1) {
		return;
	}

	if (index < 2 && !isAlock) {              /* <alock>后只能跟1，<lock>后接2,3,4,5,6  */
		return;
	}

	if (strlen(&pcontent[1]) != 11) {
		return;
	}
	__setUser(index, &pcontent[1]);
	__storeUSERParam();
	
	if (pcontent[1] != '1') {
		return;
	}

	sms = pvPortMalloc(60);

	memcpy(sms, __toUser1, sizeof(__toUser1));
	sms[5] = index + '0';
	for (i = 0; i < 11; i++) {
		sms[sizeof(__toUser1) + 2 * i] = 0;
		sms[sizeof(__toUser1) + 2 * i + 1] = pcontent[1 + i];
	}
	__sendToUser(1, sms, 11 * 2 + sizeof(__toUser1));

	memcpy(sms, __toUser, sizeof(__toUser));
	sms[11] = index + '0';
	__sendToUser(index, sms, sizeof(__toUser));
	vPortFree(sms);
}

static void __cmd_UNLOCK_Handler(const SMSInfo *p) {
	const char *pcontent = (const char *)p->content;
	int index;
	if (p->contentLen > 9) {
		return;
	}
	if ((pcontent[8] >= '7') || (pcontent[8] <= '1')) {
		return;
	}
	index = pcontent[8] - '0';
	SMSCmdRemoveUser(index);

}

static void __cmd_AHQX_Handler(const SMSInfo *p) {
}

static void __cmd_SMSC_Handler(const SMSInfo *p) {
}

static void __cmd_CLR_Handler(const SMSInfo *p) {
}

static void __cmd_DM_Handler(const SMSInfo *p) {
}

static void __cmd_DSP_Handler(const SMSInfo *p) {
}

static void __cmd_STAY_Handler(const SMSInfo *p) {         
}

static void __cmd_YSP_Handler(const SMSInfo *p) {            /*设置播报语速*/
	const char *pcontent = (const char *)p->content;
  char v = atoi(&pcontent[5]) + '0';
	if((v < '0')||(v > '9')){
		return;
	}

}

static void __cmd_YM_Handler(const SMSInfo *p) {             /*设置播报声音类型*/
	const char *pcontent = (const char *)p->content;
  char v = atoi(&pcontent[4]) + '0';
	if((v < '3')||(v > '5')){
		return;
	}

}

static void __cmd_YD_Handler(const SMSInfo *p) {              /*设置播报语调*/
	const char *pcontent = (const char *)p->content;
  char v = atoi(&pcontent[4]) + '0';
	if((v < '0')||(v > '9')){
		return;
	}

}

static void __cmd_VOLUME_Handler(const SMSInfo *p) {          /*设置播放音量*/
	const char *pcontent = (const char *)p->content;
  char v = atoi(&pcontent[8]) + '0';
	if((v < '0')||(v > '9')){
		return;
	}
}

static void __cmd_INT_Handler(const SMSInfo *p) {               /*设置播报停顿时间*/
	const char *pcontent = (const char *)p->content;
  int v = atoi(&pcontent[5]);
	if((v < 0)||(v > 99)){
		return;
	}

}

static void __cmd_YC_Handler(const SMSInfo *p) {                 /*设置播报次数*/
	const char *pcontent = (const char *)p->content;
  int v = atoi(&pcontent[4]);
	if((v < 0)||(v > 99)){
		return;
	}

}

static void __cmd_R_Handler(const SMSInfo *p) {
}

static void __cmd_VALID_Handler(const SMSInfo *p) {
}

static void __cmd_USER_Handler(const SMSInfo *p) {             /*查询权限用户*/
	unsigned char *a;
	char *buf, *pdu;
	int len;
	buf = pvPortMalloc(90);
	pdu = pvPortMalloc(128);
	sprintf(buf, "<USER>%s", USERpara(a));
	vPortFree(a);
	len = SMSEncodePdu8bit(pdu, (const char *)p->number, buf);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
	vPortFree(buf);
}

extern char *fix(void);

static inline void __storeMountTime(char *p) {
	NorFlashWrite(FIX_PARAM_STORE_ADDR, (const short *)p, 40);
}

static unsigned char mount[32];

                                   /*安装测试*/
static void __cmd_ST_Handler(const SMSInfo *p) {                  /*安装时间*/

}


static void __cmd_MOUNTER_Handler(const SMSInfo *p) {             /*查询安装人员及地点*/
  const char *t = (const char *)(Bank1_NOR2_ADDR + FIX_PARAM_STORE_ADDR);
  char *pdu = pvPortMalloc(64);
	int len;
	  
  len = SMSEncodePduUCS2(pdu, p->number, &t[8], 32);
  GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
}

static void __cmd_REST_Handler(const SMSInfo *p){                 /*取消安装时间*/
	NorFlashMutexLock(configTICK_RATE_HZ * 4);
	FSMC_NOR_EraseSector(FIX_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	NorFlashMutexUnlock();
}

static void __cmd_ERR_Handler(const SMSInfo *p) {
}

static void __cmd_ADMIN_Handler(const SMSInfo *p) {                 /*查询一号用户 */
	char buf[24];
	int len;
	char *pdu;
	if (NULL == __user(1)) {
		sprintf(buf, "<USER><1>%s", "EMPTY");
	} else {
		sprintf(buf, "<USER><1>%s", __user(1));
	}
	pdu = pvPortMalloc(300);
	len = SMSEncodePdu8bit(pdu, (const char *)p->number, buf);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
}

static void __cmd_IMEI_Handler(const SMSInfo *p) {                    /*查询手机模块IMEI*/
	char buf[16];
	int len;
	char *pdu;

	sprintf(buf, "<IMEI>%s", GsmGetIMEI());
	pdu = pvPortMalloc(300);
	len = SMSEncodePdu8bit(pdu, (const char *)p->number, buf);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
}

static void __cmd_REFAC_Handler(const SMSInfo *p) {                    /*恢复出厂设置*/
	NorFlashMutexLock(configTICK_RATE_HZ * 4);
	FSMC_NOR_EraseSector(XFS_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(GSM_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(USER_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(SMS1_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	FSMC_NOR_EraseSector(FLAG_PARAM_STORE_ADDR);
	vTaskDelay(configTICK_RATE_HZ / 5);
	NorFlashMutexUnlock();
	printf("Reboot From Default Configuration\n");
	NVIC_SystemReset();
}

static void __cmd_RST_Handler(const SMSInfo *p) {                  /*重启*/
   	printf("Reset From Default Configuration\n");
	NVIC_SystemReset();
}

extern unsigned char *Gsmpara(unsigned char *p);

static void __cmd_TEST_Handler(const SMSInfo *p) {                 /*查询当前所有参数*/
	unsigned char len;
	unsigned char *a;
	char *buf, *pdu, *time;
	char dat[9] = {0};
	const char *t = (const char *)(Bank1_NOR2_ADDR + FIX_PARAM_STORE_ADDR);
	time = pvPortMalloc(32);
	sprintf(time, t);
	buf = pvPortMalloc(300);
	pdu = pvPortMalloc(600);
	vPortFree(a);
	vPortFree(a);
	len += sprintf(&buf[len], "#V.%s", __TARGET_STRING__);
//	len += sprintf(&buf[len], "IMEI%s.", GsmGetIMEI());
	if(time[0] != 0xff){
		memcpy(dat, time, 8);
	  len += sprintf(&buf[len], "#D%s", dat);
	}
	len += sprintf(&buf[len], "#U%s", USERpara(a));
	vPortFree(a);
	len = SMSEncodePdu8bit(pdu, (const char *)p->number, buf);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
	vPortFree(buf);
}

static char NUM[15] = {0};
extern char *isChina(void);

static void __cmd_QUERYFARE_Handler(const SMSInfo *p) {            /*查询当前话费余额*/

}

static void __cmd_QUERYALL_Handler(const SMSInfo *p) {            /*发送短信到10086*/

}

static void __cmd_QUERYFLOW_Handler(const SMSInfo *p) {              /*查询当前GPRS流量*/

}

static void __cmd_QUERYMENU_Handler(const SMSInfo *p){                 /*查询手机套餐*/

}

static char __cmd_BUSINESS_Handler(const SMSInfo *p) {              /*设置查询短信内容*/
	char *pcontent = (char *)p->content;
	char i;
	if (pcontent[6] == '1') {	
		memset(__business.mobilefare, 0, 10);
		strcpy(__business.mobilefare, &pcontent[7]);
		__storeBUSIParam();
	} else if(pcontent[6] == '2') {
		memset(__business.unicomfare, 0, 10);
		strcpy(__business.mobilefare, &pcontent[7]);
		__storeBUSIParam();
	} else if(pcontent[6] == '3') {
		memset(__business.mobileflow, 0, 10);
		strcpy(__business.mobilefare, &pcontent[7]);
		__storeBUSIParam();
	} else if(pcontent[6] == '4') {
		memset(__business.unicomflow, 0, 10);
		strcpy(__business.mobilefare, &pcontent[7]);
		__storeBUSIParam();
	} else if(pcontent[6] == '5') {
		memset(__business.mobilemenu, 0, 10);
		strcpy(__business.mobilefare, &pcontent[7]);
		__storeBUSIParam();
	} else if(pcontent[6] == '6') {
		memset(__business.unicommenu, 0, 10);
		strcpy(__business.mobilefare, &pcontent[7]);
		__storeBUSIParam();
	} 
}

static void __cmd_SETIP_Handler(const SMSInfo *p) {               /*重置TCP连接的IP及端口号*/
	char *pcontent = (char *)p->content;
	if(pcontent[7] != 0x22){
	   return;
	}
  GsmTaskSendSMS(pcontent, strlen(pcontent));
}

static void __cmd_SETSPACING_Handler(const SMSInfo *p) {              /*设置重新播报间隔时间*/
	char *pcontent = (char *)p->content;
	char plen = p->contentLen;
	if((plen > 7) || (plen < 6)){
		return;
	}
	if((plen == 6) && ((pcontent[5] > '9') || (pcontent[5] < '1'))){
	   return;
	}

	if((plen == 7) && ((pcontent[6] > '9') || (pcontent[6] < '0'))){
	   return;
	}
  GsmTaskSendSMS(pcontent, strlen(pcontent));
}

static void __cmd_SETFREQUENCE_Handler(const SMSInfo *p) {             /*设置重新播报次数*/ 
	char *pcontent = (char *)p->content;
	char plen = p->contentLen;
	if((plen > 8) || (plen < 7)){
		return;
	}
	if((plen == 7) && ((pcontent[6] > '9') || (pcontent[6] < '1'))){
	   return;
	}
	
	if((plen == 8) && ((pcontent[7] > '9') || (pcontent[7] < '0'))){
	   return;
	}
  GsmTaskSendSMS(pcontent, strlen(pcontent));
}

static void __cmd_UPDATA_Handler(const SMSInfo *p) {              /*升级固件程序*/
	int i;
	int j = 0;
	FirmwareUpdaterMark *mark;
	char *pcontent = (char *)p->content;
	char *host = (char *)&pcontent[8];
	char *buff[3] = {0, 0, 0};

	for (i = 10; pcontent[i] != 0; ++i) {
		if (pcontent[i] == ',') {
			pcontent[i] = 0;
			++i;
			if (j < 3) {
				buff[j++] = (char *)&pcontent[i];
			}
		}
	}

	if (j != 3) {
		return;
	}

	mark = pvPortMalloc(sizeof(*mark));
	if (mark == NULL) {
		return;
	}

	if (FirmwareUpdateSetMark(mark, host, atoi(buff[0]), buff[1], buff[2])) {
		NorFlashMutexLock(configTICK_RATE_HZ * 4);
	  FSMC_NOR_EraseSector(GSM_PARAM_STORE_ADDR);
	  vTaskDelay(configTICK_RATE_HZ / 2);
		FSMC_NOR_EraseSector(FLAG_PARAM_STORE_ADDR);
	  vTaskDelay(configTICK_RATE_HZ / 2);
		FSMC_NOR_EraseSector(XFS_PARAM_STORE_ADDR);
	  vTaskDelay(configTICK_RATE_HZ / 2);
	  NorFlashMutexUnlock();	
		NVIC_SystemReset();
	}
	vPortFree(mark);
}

static void __cmd_FMC_Handler(const SMSInfo *sms){                  /*关闭FM*/
	SoundControlSetChannel(SOUND_CONTROL_CHANNEL_FM, 0);
}


static void __cmd_FMO_Handler(const SMSInfo *sms){                  /*打开FM调频*/
	const char *pcontent = (const char *)sms->content;
	fmopen(atoi(&pcontent[5]));
}

static void __cmd_VERSION_Handler(const SMSInfo *sms) {             /*查询当前固件版本*/
  char *pdu;
	int len;
	const char *version = Version();
	pdu = pvPortMalloc(100);
	len = SMSEncodePdu8bit(pdu, sms->number,(char *)version);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
}


// void __cmd_WARNING_Handler(const SMSInfo *sms){
// 	const char *pcontent = (const char *)sms->content;
// 	int plen = sms->contentLen;
// 	XfsTaskSpeakUCS2((const char *)&pcontent[3], (plen - 3));
// }

static void __cmd_CTCP_Handler(const SMSInfo *sms){                 /*选择是否开启GPRS连接*/
	const char *p = (const char *)sms->content;
	int plen = sms->contentLen;
	if((p[6] != '1') && (p[6] != '0')){
	   return;
	}

	if(strlen(p) > 7){
	   return;
	}
	GsmTaskSetParameter(p, plen);
}


static void __cmd_QUIET_Handler(const SMSInfo *sms){                /*选择是否开启每日静音时间*/
    const char *p = (const char *)sms->content;
	int plen = sms->contentLen;															 
	if((p[7] != '1') && (p[7] != '0')){
	   return;
	}
	GsmTaskSetParameter(p, plen);

	if(strlen(p) != 13){
	   return;
	}

	if(p[8] != ','){
	   return;
	}

	if((p[9] >= '0') && (p[9] <= '9') && (p[10] >= '0') && (p[10] <= '9') &&
	    (p[11] >= '0') && (p[11] <= '9') && (p[12] >= '0') && (p[12] <= '9')){
	   GsmTaskSendSMS(p, plen);	
	}
} 


#define UP1 (1 << 1)
#define UP2 (1 << 2)
#define UP3 (1 << 3)
#define UP4 (1 << 4)
#define UP5 (1 << 5)
#define UP6 (1 << 6)
#define UP_ALL (1<<7)


typedef struct {
	char *cmd;
	void (*smsCommandFunc)(const SMSInfo *p);
	uint32_t permission;
} SMSModifyMap;

const static SMSModifyMap __SMSModifyMap[] = {
	{"<LOCK>", __cmd_LOCK_Handler, UP1},
	{"<ALOCK>", __cmd_LOCK_Handler, UP1},
	{"<UNLOCK>", __cmd_UNLOCK_Handler, UP1},
	{"<AHQXZYTXXZX>", __cmd_AHQX_Handler, UP_ALL},
	{"<SMSC>", __cmd_SMSC_Handler, UP_ALL},
	{"<CLR>", __cmd_CLR_Handler, UP_ALL},
	{"<DM>", __cmd_DM_Handler, UP_ALL},
	{"<DSP>", __cmd_DSP_Handler, UP_ALL},
	{"<STAY>", __cmd_STAY_Handler, UP_ALL},
	{"<YSP>", __cmd_YSP_Handler, UP_ALL},
	{"<YM>", __cmd_YM_Handler, UP_ALL},
	{"<YD>", __cmd_YD_Handler, UP_ALL},
	{"<VOLUME>", __cmd_VOLUME_Handler, UP_ALL},
	{"<INT>", __cmd_INT_Handler, UP_ALL},
	{"<YC>", __cmd_YC_Handler, UP_ALL},
	{"<R>", __cmd_R_Handler, UP_ALL},
	{"<VALID>", __cmd_VALID_Handler, UP_ALL},
	{"<USER>", __cmd_USER_Handler, UP_ALL},
//	{"<ST>", __cmd_ST_Handler, UP_ALL},
  {"<MOUNT>", __cmd_MOUNTER_Handler, UP_ALL},
  {"<REST>", __cmd_REST_Handler, UP_ALL},
	{"<ERR>", __cmd_ERR_Handler, UP_ALL},
	{"<ADMIN>", __cmd_ADMIN_Handler, UP_ALL},
	{"<IMEI>", __cmd_IMEI_Handler, UP_ALL},
	{"<REFAC>", __cmd_REFAC_Handler, UP_ALL},
	{"<RST>", __cmd_RST_Handler, UP_ALL},
	{"<TEST>", __cmd_TEST_Handler, UP_ALL},
	{"<UPDATA>", __cmd_UPDATA_Handler, UP_ALL},
	{"<SETIP>", __cmd_SETIP_Handler, UP_ALL},
	{"<SPA>", __cmd_SETSPACING_Handler, UP_ALL},
	{"<FREQ>", __cmd_SETFREQUENCE_Handler, UP_ALL},
	{"<QU>", __cmd_QUERYFARE_Handler, UP_ALL},
	{"<FLOW>", __cmd_QUERYFLOW_Handler, UP_ALL},
	{"<MENU>", __cmd_QUERYMENU_Handler, UP_ALL},
	{"<ALL>", __cmd_QUERYALL_Handler, UP_ALL},
	{"<BUSI>", __cmd_BUSINESS_Handler, UP_ALL},
   
	{"<FMO>",  __cmd_FMO_Handler,  UP_ALL}, 
	{"<FMC>",  __cmd_FMC_Handler,  UP_ALL}, 

	{"<VERSION>", __cmd_VERSION_Handler, UP_ALL},
	{"<CTCP>",  __cmd_CTCP_Handler, UP_ALL},
	{"<QUIET>", __cmd_QUIET_Handler, UP_ALL},
	{NULL, NULL},
};

static int smslen = 0;

int *oflen(void){
	return &smslen;
}

static char repeat = 0;

char *smsrep(void){
	return &repeat;
}

static char NoIndex[] = {0x97, 0x5E, 0x63, 0x88, 0x67, 0x43, 0x53, 0xF7, 0x78, 0x01, 0x4F, 0xE1, 0x60, 0x6F};    /*非授权号码信息*/

void ProtocolHandlerSMS(const SMSInfo *sms) {
	const SMSModifyMap *map;
	int index;
	const char *pnumber = (const char *)sms->number;
  char *pcontent = (char *)sms->content;
	int plen = sms->contentLen;	

  __restorBUSIParam();	
	
	if((strncmp(pnumber, "10086", 5) == 0) || (strncmp(pnumber, "10010", 5) == 0)){
		int len, i;
		char *pdu = pvPortMalloc(300);
		char t;
		for(i = 0; i < (plen/2); i++) {
			t = pcontent[2 * i];
			pcontent[2 * i] = pcontent[2 * i + 1];
			pcontent[2 * i + 1] = t;
		}
		
		if(NUM[0] == 0){
			return;
		}
	
		if(plen <= 140){
			len = SMSEncodePduUCS2(pdu, NUM, pcontent, plen);
			GsmTaskSendSMS(pdu, len);
			vPortFree(pdu);
			return;
		} else if((plen > 140) && (plen <= 280)){
			len = SMSEncodePduUCS2(pdu, NUM, pcontent, 140);
			GsmTaskSendSMS(pdu, len);
			vPortFree(pdu);
			
			vTaskDelay(configTICK_RATE_HZ * 2);
			len = 0;
			pdu = pvPortMalloc(300);
			len = SMSEncodePduUCS2(pdu, NUM, &pcontent[140], (plen - 140));
			GsmTaskSendSMS(pdu, len);
			vPortFree(pdu);
			return;
		} else if(plen > 280){
			len = SMSEncodePduUCS2(pdu, NUM, pcontent, 140);
			GsmTaskSendSMS(pdu, len);
			vPortFree(pdu);
			
			vTaskDelay(configTICK_RATE_HZ * 2);
			len = 0;
			pdu = pvPortMalloc(300);
			len = SMSEncodePduUCS2(pdu, NUM, &pcontent[140], 140);
			GsmTaskSendSMS(pdu, len);
			vPortFree(pdu);
			
			vTaskDelay(configTICK_RATE_HZ * 2);
			len = 0;
			pdu = pvPortMalloc(300);
			len = SMSEncodePduUCS2(pdu, NUM, &pcontent[280], (plen - 280));
			GsmTaskSendSMS(pdu, len);	
			vPortFree(pdu);
			return;
		}
	}
	
	if((pcontent[0] == '<') && (((*(pcontent + 2)) == 's') || ((*(pcontent + 2)) == 'S')) && 
		(((*(pcontent + 4)) == 't') || ((*(pcontent + 4)) == 'T')) && ((*(pcontent + 6)) == '>')){
	  memcpy(mount, (pcontent + 8), (plen - 8));
	  __cmd_ST_Handler(sms);
		return;
	}
	
	index = __userIndex(sms->numberType == PDU_NUMBER_TYPE_INTERNATIONAL ? &pnumber[2] : &pnumber[0]);
	for (map = __SMSModifyMap; map->cmd != NULL; ++map) {
		if (strncasecmp((const char *)sms->content, map->cmd, strlen(map->cmd)) == 0) {
			if (map->permission != UP_ALL) {
				if ((map->permission & (1 << (index))) == 0) {
					return;
				}
			}

			map->smsCommandFunc(sms);
			return;
		}
	}

//	SoundControlSetChannel(SOUND_CONTROL_CHANNEL_XFS, 1);
//	GPIO_ResetBits(GPIOG, GPIO_Pin_14);
	if (index == 0) {
		return;
	}
// 	if(pcontent[2] > 0x32){
// 	   return;
//   }
  repeat = 1;
	smslen = plen + 1;
	__storeSMS1(pcontent, smslen);
}
