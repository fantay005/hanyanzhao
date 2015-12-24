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
#include "stm32f10x_gpio.h"
#include "norflash.h"
#include "zklib.h"
#include "libupdater.h"
#include "version.h"
#include "second_datetime.h"
#include "sms_cmd.h"

extern char *isCMCC(void);

typedef struct{
	char mobilefare[10];
	char unicomfare[10];
	char mobileflow[10];
	char unicomflow[10];
	char mobilemenu[10];
	char unicommenu[10];
}Business;

Business __business = {"302","66","66","5102","77"};

void __cmd_QUERYFARE_Handler(void) {            /*查询当前话费余额*/
	const char *buf = __business.mobilefare;
	const char *phoneNum = "8610086";
  char *pdu = pvPortMalloc(64);
	int len;

	len = SMSEncodePdu8bit(pdu, phoneNum, buf);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);	
}

void __cmd_QUERYFLOW_Handler(void) {              /*查询当前GPRS流量*/
	const char *buf = __business.mobileflow;
	const char *phoneNum = "8610086";
  char *pdu = pvPortMalloc(64);
	int len;

	len = SMSEncodePdu8bit(pdu, phoneNum, buf);
	GsmTaskSendSMS(pdu, len);
	vPortFree(pdu);	

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
	char *buff[2] = {0, 0};

	for (i = 10; pcontent[i] != 0; ++i) {
		if (pcontent[i] == ',') {
			pcontent[i] = 0;
			++i;
			if (j < 2) {
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

	if (FirmwareUpdateSetMark(mark, host, atoi(buff[0]), buff[1], 1)) {
		NVIC_SystemReset();
	}
	vPortFree(mark);
}

extern unsigned char *ProtocolRespond(unsigned char address[10], unsigned char  type[2], const char *msg, unsigned char *size);

void ProtocolHandlerSMS(const SMSInfo *sms) {
	const char *pnumber = (const char *)sms->number;
  char *pcontent = (char *)sms->content;
	unsigned char *buf;
	int plen = sms->contentLen;	

	if((strncmp(pnumber, "10086", 5) == 0) || (strncmp(pnumber, "10010", 5) == 0)){
		GMSParameter g;
		int i;
		unsigned char size;
		char *ret, *tmp = pvPortMalloc(plen * 2 + 1);
		char t;
		char HexToChar[] = "0123456789ABCDEF";
		
		for(i = 0; i < (plen/2); i++) {
			t = pcontent[2 * i];
			pcontent[2 * i] = pcontent[2 * i + 1];
			pcontent[2 * i + 1] = t;
		}
		
		ret = tmp;
		
		for(i = 0; i < plen; i++){
			*ret++ = HexToChar[pcontent[i] >> 4];
			*ret++ = HexToChar[pcontent[i] & 0xF];
		}	
		
		*ret++ = 0;
		
		NorFlashRead(NORFLASH_MANAGEM_ADDR, (short *)&g, (sizeof(GMSParameter) + 1) / 2);
		buf = ProtocolRespond(g.GWAddr, (unsigned char *)"25", tmp, &size);
		GsmTaskSendTcpData((const char *)buf, size);
		vPortFree((void *)buf);	
		
		vPortFree((void *)tmp);	
	}
}
