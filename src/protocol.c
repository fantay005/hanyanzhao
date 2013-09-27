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

#define WOMANSOUND  0x33
#define MANSOUND	0X32
#define MIXSOUND	0X34

extern int GsmTaskSendTcpData(const char *p, int len);
extern int GsmTaskResetSystemAfter(int seconds);

typedef struct {
	char user[6][12];
} USERParam;

USERParam __userParam;

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

typedef enum {
	TermActive  = 0x31,
	ParaSetup = 0x32,
	QueryPara = 0x33,
	Mp3File = 0x34,
	TypeChooseReply = 0x39,
} TypeChoose;

typedef enum {
	Login = 0x31,
	Heart =	0x32,

	SetupUser =	0x30,
	RemoveUser = 0x31,
	StopTime = 0x32,
	VoiceType =	0x33,
	VolumeSetup = 0x34,
	BroadTimes = 0x35,
	SendSMS	= 0x36,
	Restart = 0x37,
	ReFactory =	0x38,
	ClassificReply = 0x39,

	basic =	0x31,
	Coordinate = 0x32,

	Record	= 0x31,
	SMSPrompt = 0x32,
	RecordPrompt = 0x33,
	Mp3Music = 0x34,
	LongSMS = 0x35,

} Classific;

typedef struct {
	unsigned char header[2];
	unsigned char lenH;
	unsigned char lenL;
	unsigned char type;
	unsigned char class;
	unsigned short radom;
	unsigned short reserve;
} ProtocolHeader;

typedef struct {
	unsigned char sum[2];
	unsigned char x0D;
	unsigned char x0A;
} ProtocolPadder;

void ProtocolDestroyMessage(const char *p) {
	vPortFree((void *)p);
}

static char HexToChar(unsigned char hex) {
	unsigned char hexTable[] = "0123456789ABCDEF";
	return hexTable[hex & 0x0F];
}

char *ProtocolMessage(TypeChoose type, Classific class, const char *message, int *size) {
	int i;
	unsigned char sum = 0;
	unsigned char *p, *ret;
	int len = message == NULL ? 0 : strlen(message);

	*size = sizeof(ProtocolHeader) + len + sizeof(ProtocolPadder);
	ret = pvPortMalloc(*size);
	{
		ProtocolHeader *h = (ProtocolHeader *)ret;
		h->header[0] = '#';
		h->header[1] = 'H';
		h->lenH = len >> 8;
		h->lenL = len;
		h->type = type;
		h->class = class;
		h->radom = 0x3030;
		h->reserve = 0x3030;
	}

	if (message != NULL) {
		strcpy((char *)(ret + sizeof(ProtocolHeader)), message);
	}

	p = ret;
	for (i = 0; i < len + sizeof(ProtocolHeader); ++i) {
		sum += *p++;
	}

	*p++ = HexToChar(sum >> 4);
	*p++ = HexToChar(sum);
	*p++ = 0x0D;
	*p = 0x0A;
	return (char *)ret;
}

void SoftReset(void) {
	__set_FAULTMASK(1);  //关闭所有终端
	NVIC_SystemReset();	 //复位
}

char *ProtoclCreatLogin(char *imei, int *size) {
	return ProtocolMessage(TermActive, Login, imei, size);
}

char *ProtoclCreateHeartBeat(int *size) {
	return ProtocolMessage(TermActive, Heart, NULL, size);
}

char *TerminalCreateFeedback(const char radom[4], int *size) {
	char r[5];
	r[0] = radom[0];
	r[1] = radom[1];
	r[2] = radom[2];
	r[3] = radom[3];
	r[4] = 0;
	return ProtocolMessage(TypeChooseReply, ClassificReply, r, size);
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
}

void __cmd_ALARM_Handler(const sms_t *p) {
	const char *pcontent = p->sms_content;

	switch (pcontent[7]) {
	case 1 : {
		GPIO_SetBits(GPIOA, GPIO_Pin_4);
		break;
	}

	case 2 : {
		GPIO_SetBits(GPIOA, GPIO_Pin_5);
		break;
	}

	case 3 : {
		GPIO_SetBits(GPIOA, GPIO_Pin_6);
		break;
	}

	case 4 : {
		GPIO_SetBits(GPIOA, GPIO_Pin_7);
		break;
	}
	case 0: {
		GPIO_ResetBits(GPIOA, GPIO_Pin_4);
		GPIO_ResetBits(GPIOA, GPIO_Pin_5);
		GPIO_ResetBits(GPIOA, GPIO_Pin_6);
		GPIO_ResetBits(GPIOA, GPIO_Pin_7);
		return;
	}
	default : {
		return;
	}
	}
	LedDisplayGB2312String162(0, 0, &pcontent[8]);
	LedDisplayToScan(0, 0, 16, 15);
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


	// 显示短信内容

}


typedef void (*ProtocolHandleFunction)(ProtocolHeader *header, char *p);
typedef struct {
	unsigned char type;
	unsigned char class;
	ProtocolHandleFunction func;
} ProtocolHandleMap;


void HandleLogin(ProtocolHeader *header, char *p) {
	ProtocolDestroyMessage(p);
}

void HandleHeartBeat(ProtocolHeader *header, char *p) {
	ProtocolDestroyMessage(p);
}

void HandleSettingUser(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleRemoveUser(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleDeadTime(ProtocolHeader *header, char *p) {
	int len;
	int choose;
	choose = (p[1] - '0') * 10 + (p[0] - '0');
	XfsTaskSetSpeakPause(choose);
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleVoiceType(ProtocolHeader *header, char *p) {
	int len,  choose;
	choose = *p;
	if (choose == 0x34) {
		choose = MANSOUND;
	} else if (choose == 0x35) {
		choose = MIXSOUND;
	} else if (choose == 0x33) {
		choose = WOMANSOUND;
	}
	XfsTaskSetSpeakType(choose);
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleVolumeSetting(ProtocolHeader *header, char *p) {
	int len,  choose;
	choose = *p;
	XfsTaskSetSpeakVolume(choose);
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleBroadcastTimes(ProtocolHeader *header, char *p) {
	int len, times;
	times = (p[1] - '0') * 10 + (p[0] - '0');
	XfsTaskSetSpeakTimes(times);
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleSendSMS(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
#if defined(__SPEAKER__)
	XfsTaskSpeakUCS2(p, len);
#elif defined(__LED__)
	MessDisplay(p);
#endif
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
	return;
}

void HandleRestart(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
	GsmTaskResetSystemAfter(10);
}


void HandleRecoverFactory(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleBasicParameter(ProtocolHeader *header, char *p) {

	ProtocolDestroyMessage(p);
}

void HandleCoordinate(ProtocolHeader *header, char *p) {
	ProtocolDestroyMessage(p);
}

void HandleRecordMP3(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleSMSPromptSound(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleRecordPromptSound(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleMP3Music(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleLongSMS(ProtocolHeader *header, char *p) {
	int len;
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}


void ProtocolHandler(char *p) {
//	if (strncmp(p, "#H", 2) != 0) return;
	int i;
	const static ProtocolHandleMap map[] = {
		{'1', '1', HandleLogin},
		{'1', '2', HandleHeartBeat},
		{'2', '0', HandleSettingUser},
		{'2', '1', HandleRemoveUser},
		{'2', '2', HandleDeadTime},
		{'2', '3', HandleVoiceType},
		{'2', '4', HandleVolumeSetting},
		{'2', '5', HandleBroadcastTimes},
		{'2', '6', HandleSendSMS},
		{'2', '7', HandleRestart},
		{'2', '8', HandleRecoverFactory},
		{'3', '1', HandleBasicParameter},
		{'3', '2', HandleCoordinate},
		{'4', '1', HandleRecordMP3},
		{'4', '2', HandleSMSPromptSound},
		{'4', '3', HandleRecordPromptSound},
		{'4', '4', HandleMP3Music},
		{'4', '5', HandleLongSMS},
	};
	ProtocolHeader *header = (ProtocolHeader *)p;
//	int len = (header->lenH << 8) + header->lenL;
//	printf("sizeof(ProtocolHeader)=%d\n", sizeof(ProtocolHeader));
//	printf("Protocol:\nlen=%d\n", len);
//	printf("type=%c\n", header->type);
//	printf("class=%c\n", header->class);
//	printf("content=%s", p + sizeof(ProtocolHeader));

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if ((map[i].type == header->type) && (map[i].class == header->class)) {
			map[i].func(header, p + sizeof(ProtocolHeader));
			break;
		}
	}
}


