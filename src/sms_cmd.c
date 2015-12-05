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

extern char *isCMCC(void);
static char NUM[15] = {0};

typedef struct{
	char mobilefare[10];
	char unicomfare[10];
	char mobileflow[10];
	char unicomflow[10];
	char mobilemenu[10];
	char unicommenu[10];
}Business;

Business __business = {"302","101","703","5102","77"};

void __cmd_QUERYFARE_Handler(const SMSInfo *p) {            /*查询当前话费余额*/
	const char *pnumber = (const char *)p->number;
	char plen = p->contentLen;
	int len; 
  char *pdu = pvPortMalloc(64);
	if(plen != 4){
	  return;
	}
	if(*isCMCC() == 1){
		const char *buf = __business.mobilefare;
		const char *phoneNum = "8610086";
		//memset(NUM, 0 , 15);
		sprintf(NUM, pnumber);
		len = SMSEncodePdu8bit(pdu, phoneNum, buf);
		GsmTaskSendSMS(pdu, len);
		vPortFree(pdu);	
	} else if (*isCMCC() == 2){
		const char *buf = __business.unicomfare;
		const char *phoneNum = "8610010";
		//memset(NUM, 0 , 15);
		sprintf(NUM, pnumber);
		len = SMSEncodePdu8bit(pdu, phoneNum, buf);
		GsmTaskSendSMS(pdu, len);
		vPortFree(pdu);
	} else {
		return;
	}
}

void __cmd_QUERYFLOW_Handler(const SMSInfo *p) {              /*查询当前GPRS流量*/
	const char *pnumber = (const char *)p->number;
	char plen = p->contentLen;
	int len; 
  char *pdu = pvPortMalloc(64);
	if(plen > 6){
	  return;
	}
	if(*isCMCC() == 1){
		const char *buf = __business.mobileflow;
		const char *phoneNum = "8610086";
		//memset(NUM, 0 , 15);
		sprintf(NUM, pnumber);
		len = SMSEncodePdu8bit(pdu, phoneNum, buf);
		GsmTaskSendSMS(pdu, len);
		vPortFree(pdu);	
	} else if (*isCMCC() == 2){
		const char *buf = __business.unicomflow;
		const char *phoneNum = "8610010";
		//memset(NUM, 0 , 15);
		sprintf(NUM, pnumber);
		len = SMSEncodePdu8bit(pdu, phoneNum, buf);
		GsmTaskSendSMS(pdu, len);
		vPortFree(pdu);
	} else {
		return;
	}
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

	if (FirmwareUpdateSetMark(mark, host, atoi(buff[0]), buff[1])) {
		NVIC_SystemReset();
	}
	vPortFree(mark);
}

void ProtocolHandlerSMS(const SMSInfo *sms) {
	const char *pnumber = (const char *)sms->number;
  char *pcontent = (char *)sms->content;
	int plen = sms->contentLen;	

	if((strncmp(pnumber, "10086", 5) == 0) || (strncmp(pnumber, "10010", 5) == 0)){
		int len, i;
		char *pdu = pvPortMalloc(300);
		char t;
		for(i = 0; i < (plen/2); i++) {
			t = pcontent[2 * i];
			pcontent[2 * i] = pcontent[2 * i + 1];
			pcontent[2 * i + 1] = t;
		}
		
		if(NUM[0] == 0)
			return;
	
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
	
}
