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
#include "zklib.h"

static void __cmd_SETIP_Handler(const SMSInfo *p) {               /*重置TCP连接的IP及端口号*/
	char *pcontent = (char *)p->content;
	if(pcontent[7] != 0x22){
	   return;
	}
  GsmTaskSendSMS(pcontent, strlen(pcontent));
}

static void __cmd_UPDATA_Handler(const SMSInfo *p) {              /*升级固件程序*/

}

typedef struct {
	char *cmd;
	void (*smsCommandFunc)(const SMSInfo *p);
} SMSModifyMap;

const static SMSModifyMap __SMSModifyMap[] = {
	{"<UPDATA>", __cmd_UPDATA_Handler},
	{"<SETIP>", __cmd_SETIP_Handler},  
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
