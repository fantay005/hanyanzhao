#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "protocol.h"
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

typedef struct {
	char user[6][12];
} USERParam;

USERParam __userParam;

const uint8_t clear[72] = {0};

static int __userIndex(const char *user) {
	int i;
	for (i = 0; i < ARRAY_MEMBER_NUMBER(__userParam.user) ; ++i) {
		if (strcmp(user, __userParam.user[i]) == 0) {
			return i + 1;
		}
	}
	return 0;
}

static void __setUser(int index, const char *user) {
	strcpy(__userParam.user[index], user);
}

static inline void __storeUSERParam(void) {
	NorFlashWrite(USER_PARAM_STORE_ADDR, (const short *)&__userParam, sizeof(__userParam));
}

void restorUSERParam(void) {
	NorFlashRead(USER_PARAM_STORE_ADDR, (short *)&__userParam, sizeof(__userParam));
}

void sendToUser1(sms_t *p) {
	const char *pcontent = p->sms_content;
	unsigned char pcontent_len = p->content_len;
	const char *pnumber = p->number;
	int i;
	int sms_buff[] = {0X5D, 0XF2, 0x5C,	0x06, 0x00, 0x00, 0x53, 0xF7, 0x75,
					  0x28, 0x62, 0x37, 0x63, 0x88, 0x67, 0x43, 0x4E, 0x0E
					 };

	sms_buff[5] = pcontent[6];
	for (i = 0; i < (pcontent_len - 7); i++) {
		sms_buff[18 + 2 * i] = 0;
		sms_buff[18 + 2 * i + 1] = pcontent[7 + i];
	}

}

void __cmd_LOCK_Handler(sms_t *p) {
	const char *pcontent = p->sms_content;

	int index =  __userIndex(p->number_type == PDU_NUMBER_TYPE_NATIONAL ? p->number : &p->number[2]);
	if (index != 1) {
		return;
	}
	index = pcontent[6] - '0';
	if (index < 2 || index > 6) {
		return;
	}
	if (__userParam.user[index][0] != 1) {
		return;
	}
	if (strlen(&pcontent[7]) != 11) {
		return;
	}
	__setUser(index, &pcontent[7]);
	__storeUSERParam();

}

void __cmd_ALOCK_Handler(const sms_t *p) {
}

void __cmd_UNLOCK_Handler(const sms_t *p) {
}

void __cmd_AHQX_Handler(const sms_t *p) {
}

void __cmd_SMSC_Handler(const sms_t *p) {
}

void __cmd_CLR_Handler(const sms_t *p) {
}

void __cmd_DM_Handler(const sms_t *p) {
}

void __cmd_DSP_Handler(const sms_t *p) {
}

void __cmd_STAY_Handler(const sms_t *p) {
}

void __cmd_YSP_Handler(const sms_t *p) {
}

void __cmd_YM_Handler(const sms_t *p) {
}

void __cmd_YD_Handler(const sms_t *p) {
}

void __cmd_VOLUME_Handler(const sms_t *p) {
}

void __cmd_INT_Handler(const sms_t *p) {
}

void __cmd_YC_Handler(const sms_t *p) {
}

void __cmd_R_Handler(const sms_t *p) {
}

void __cmd_VALID_Handler(const sms_t *p) {
}

void __cmd_USER_Handler(const sms_t *p) {
}

void __cmd_ST_Handler(const sms_t *p) {
}

void __cmd_ERR_Handler(const sms_t *p) {
}

void __cmd_ADMIN_Handler(const sms_t *p) {
}

void __cmd_IMEI_Handler(const sms_t *p) {
}

void __cmd_REFAC_Handler(const sms_t *p) {
}

void __cmd_RES_Handler(const sms_t *p) {
}

void __cmd_TEST_Handler(const sms_t *p) {
}

void __cmd_SETIP_Handler(const sms_t *p) {
}

void __cmd_UPDATA_Handler(const sms_t *p) {
	int i;
	int j = 0;
	FirmwareUpdaterMark *mark;
	char *pcontent = (char *)p->sms_content;
	char *host = (char *)&pcontent[9];
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

	if (FirmwareUpdateSetMark(mark, host, atoi(&buff[0]), &buff[1], &buff[2])) {
		NVIC_SystemReset();
	}
	vPortFree(mark);
}

void __cmd_ALARM_Handler(const sms_t *p) {
	const char *pcontent = p->sms_content;
	enum SoftPWNLedColor color;
	switch (pcontent[7]) {
	case '1' : {
		color = SoftPWNLedColorYellow;
		break;
	}

	case '2' : {
		color = SoftPWNLedColorOrange;
		break;
	}

	case '3' : {
		color = SoftPWNLedColorBlue;
		break;
	}

	case '4' : {
		color = SoftPWNLedColorRed;
		break;
	}
	case 0: {
		color =	SoftPWNLedColorNULL;
		break;
	}
	default : {
		color =	SoftPWNLedColorNULL;
		break;
	}
	}
	SoftPWNLedSetColor(color);
	LedDisplayGB2312String162(0, 0, &pcontent[8]);
	LedDisplayToScan2(0, 0, 7, 15);
}

void __cmd_RED_Display(const sms_t *sms) {
	const char *pcontent = sms->sms_content;
	int plen = sms->content_len;
	XfsTaskSpeakUCS2(&pcontent[2], (plen - 1));
	DisplayClear();
	if (sms->encode_type == ENCODE_TYPE_UCS2) {
		uint8_t *gbk = Unicode2GBK(&pcontent[2], (plen - 1));
		DisplayMessageRed(gbk);
		Unicode2GBKDestroy(gbk);
	} else {
		DisplayMessageRed(&pcontent[2]);
	}
}

void __cmd_GREEN_Display(const sms_t *sms) {
	const char *pcontent = sms->sms_content;
	int plen = sms->content_len;
	XfsTaskSpeakUCS2(&pcontent[2], (plen - 1));
	DisplayClear();
	if (sms->encode_type == ENCODE_TYPE_UCS2) {
		uint8_t *gbk = Unicode2GBK(&pcontent[2], (plen - 1));
		DisplayMessageGreen(gbk);
		Unicode2GBKDestroy(gbk);
	} else {
		DisplayMessageGreen(&pcontent[2]);

	}
}

void __cmd_YELLOW_Display(const sms_t *sms) {
	const char *pcontent = sms->sms_content;
	int plen = sms->content_len;
	XfsTaskSpeakUCS2(&pcontent[2], (plen - 1));
	DisplayClear();
	if (sms->encode_type == ENCODE_TYPE_UCS2) {
		uint8_t *gbk = Unicode2GBK(&pcontent[2], (plen - 1));
		DisplayMessageYELLOW(gbk);
		Unicode2GBKDestroy(gbk);
	} else {
		DisplayMessageYELLOW(&pcontent[2]);
	}
}


typedef void (*smsModifyFunction)(const sms_t *p);
typedef struct {
	char *cmd;
	smsModifyFunction MFun;
} SMSModifyMap;

const static SMSModifyMap __SMSModifyMap[] = {
	{"<LOCK>", __cmd_LOCK_Handler},
	{"<ALOCK>", __cmd_ALOCK_Handler},
	{"<UNLOCK>", __cmd_UNLOCK_Handler},
	{"<AHQXZYTXXZX>", __cmd_AHQX_Handler},
	{"<SMSC>", __cmd_SMSC_Handler},
	{"<CLR>", __cmd_CLR_Handler},
	{"<DM>", __cmd_DM_Handler},
	{"<DSP>", __cmd_DSP_Handler},
	{"<STAY>", __cmd_STAY_Handler},
	{"<YSP>", __cmd_YSP_Handler},
	{"<YM>", __cmd_YM_Handler},
	{"<YD>", __cmd_YD_Handler},
	{"<VOLUME>", __cmd_VOLUME_Handler},
	{"<INT>", __cmd_INT_Handler},
	{"<YC>", __cmd_YC_Handler},
	{"<R>", __cmd_R_Handler},
	{"<VALID>", __cmd_VALID_Handler},
	{"<USER>", __cmd_USER_Handler},
	{"<ST>", __cmd_ST_Handler},
	{"<ERR>", __cmd_ERR_Handler},
	{"<ADMIN>", __cmd_ADMIN_Handler},
	{"<IMEI>", __cmd_IMEI_Handler},
	{"<REFAC>", __cmd_REFAC_Handler},
	{"<RES>", __cmd_RES_Handler},
	{"<TEST>", __cmd_TEST_Handler},
	{"<UPDATA>", __cmd_UPDATA_Handler},
	{"<SETIP>", __cmd_SETIP_Handler},
	{"<ALARM>",	__cmd_ALARM_Handler},
	{"1", __cmd_RED_Display},
	{"2", __cmd_GREEN_Display},
	{"3", __cmd_YELLOW_Display},
	{NULL, NULL}
};


void ProtocolHandlerSMS(const sms_t *sms) {
	const SMSModifyMap *map;
	for (map = __SMSModifyMap; map->cmd != NULL; ++map) {
		if (strncmp(sms->sms_content, map->cmd, strlen(map->cmd)) == 0) {
			restorUSERParam();
			map->MFun(sms);
			return;
		}
	}
	XfsTaskSpeakUCS2(sms->sms_content, sms->content_len);
	DisplayClear();
	if (sms->encode_type == ENCODE_TYPE_UCS2) {
		uint8_t *gbk = Unicode2GBK(sms->sms_content, sms->content_len);
		LedDisplayGB2312String16(0, 0, gbk);
		Unicode2GBKDestroy(gbk);
	} else {
		LedDisplayGB2312String16(0, 0, sms->sms_content);
	}

	LedDisplayToScan(0, 0, LED_DOT_XEND, LED_DOT_YEND);
}
