#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "protocol.h"
#include "gsm.h"
#include "misc.h"
#include "sms.h"
#include "norflash.h"
#include "zklib.h"
#include "libupdater.h"
#include "version.h"


static void __cmd_REFAC_Handler(const SMSInfo *p) {                    /*恢复出厂设置*/
	NorFlashMutexLock(configTICK_RATE_HZ * 4);
	NorFlashMutexUnlock();
	printf("Reboot From Default Configuration\n");
	NVIC_SystemReset();
}

static void __cmd_RST_Handler(const SMSInfo *p) {                  /*重启*/
   	printf("Reset From Default Configuration\n");
	NVIC_SystemReset();
}


static void __cmd_SETIP_Handler(const SMSInfo *p) {               /*重置TCP连接的IP及端口号*/
	char *pcontent = (char *)p->content;
	if(pcontent[7] != 0x22){
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
	  NorFlashMutexUnlock();	
		NVIC_SystemReset();
	}
	vPortFree(mark);
}

static void __cmd_VERSION_Handler(const SMSInfo *sms) {             /*查询当前固件版本*/
  char *pdu;
	int len;
	const char *version = Version();
	pdu = pvPortMalloc(100);
	len = SMSEncodePdu8bit(pdu, (const char *)sms->number,(const char *)version);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);
}

typedef struct {
	char *cmd;
	void (*smsCommandFunc)(const SMSInfo *p);
} SMSModifyMap;

const static SMSModifyMap __SMSModifyMap[] = {
	{"<REFAC>", __cmd_REFAC_Handler},
	{"<RST>", __cmd_RST_Handler},
	{"<UPDATA>", __cmd_UPDATA_Handler},
	{"<SETIP>", __cmd_SETIP_Handler},  
	{"<VERSION>", __cmd_VERSION_Handler},
	{NULL, NULL},
};

void ProtocolHandlerSMS(const SMSInfo *sms) {
	const SMSModifyMap *map;
	int index;
	
	for (map = __SMSModifyMap; map->cmd != NULL; ++map) {
		if (strncasecmp((const char *)sms->content, map->cmd, strlen(map->cmd)) == 0) {
			map->smsCommandFunc(sms);
			return;
		}
	}

	if (index == 0) {
		return;
	}
}
