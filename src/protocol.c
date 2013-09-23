#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "stdio.h"
#include "protocol.h"
#include "string.h"
#include "xfs.h"
#include "misc.h"
#include "sms.h"
#include "display.h"

#define WOMANSOUND  0x33
#define MANSOUND	0X32
#define MIXSOUND	0X34

extern int GsmTaskSendTcpData(const char *p, int len);
extern int GsmTaskResetSystemAfter(int seconds);

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

typedef enum {
	adjVolume = 'v',
	adjTone = 't',
	adjSpeed = 's',
	adjVoice = 'm',
} adjType;

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

void AuthoUser(sms_t *p) {
}

void ForceAutho(sms_t *p) {
}

void RemoveAutho(sms_t *p) {
}

void SuperRemove(sms_t *p) {
}

void SetSMSC(sms_t *p) {
}

void SetDisp(sms_t *p) {
}

void SetDispSpeed(sms_t *p) {
}

void SetStay(sms_t *p) {
}

void SetSoundSpeed(sms_t *p) {
}

void SetSoundType(sms_t *p) {
}

void SetBroadInt(sms_t *p) {
}

void SetBraodTimes(sms_t *p) {
}

void ReturnSMSSwith(sms_t *p) {
}

void SetValidTime(sms_t *p) {
}

void QueryAutho(sms_t *p) {
}

void QueryMUMessage(sms_t *p) {
}

void QueryHitchMessage(sms_t *p) {
}

void QueryFirstUser(sms_t *p) {
}

void QueryIMEI(sms_t *p) {
}

void RestoreFactory(sms_t *p) {
}

void RestartSystem(sms_t *p) {
}

void ZKTest(sms_t *p) {
}

void RemoteUpgrade(sms_t *p) {
}

void ClearSMS(sms_t *p) {
}

void SetTone(sms_t *p) {
}

void SetVol(sms_t *p) {
}

void SetServerIP(sms_t *p) {
}

void CalamityAlarm(sms_t *p) {
}

typedef void (*smsModifyFunction)(sms_t *p);
typedef struct {
	char *cmd;
	smsModifyFunction MFun;
} SMSModifyMap;

const static SMSModifyMap __SMSModifyMap[] = {
	{"<LOCK>", AuthoUser},
	{"<ALOCK>", ForceAutho},
	{"<UNLOCK>", RemoveAutho},
	{"<AHQXZYTXXZX>", SuperRemove},
	{"<SMSC>", SetSMSC},
	{"<CLR>", ClearSMS},
	{"<DM>", SetDisp},
	{"<DSP>", SetDispSpeed},
	{"<STAY>", SetStay},
	{"<YSP>", SetSoundSpeed},
	{"<YM>", SetSoundType},
	{"<YD>", SetTone},
	{"<VOLUME>", SetVol},
	{"<INT>", SetBroadInt},
	{"<YC>", SetBraodTimes},
	{"<R>", ReturnSMSSwith},
	{"<VALID>", SetValidTime},
	{"<USER>", QueryAutho},
	{"<ST>", QueryMUMessage},
	{"<ERR>", QueryHitchMessage},
	{"<ADMIN>", QueryFirstUser},
	{"<IMEI>", QueryIMEI},
	{"<REFAC>", RestoreFactory},
	{"<RES>", RestartSystem},
	{"<TEST>", ZKTest},
	{"<UPDATA>", RemoteUpgrade},
	{"<SETIP>", SetServerIP},
	{"<ALARM>",	CalamityAlarm},
};

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
	choose = (*p++ - '0') * 10 + (*p - '0');
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

	xfsChangePara(adjVoice, choose);
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleVolumeSetting(ProtocolHeader *header, char *p) {
	int len,  choose;
	choose = *p;
	xfsChangePara(adjVolume, choose);
	len = (header->lenH << 8) + header->lenL;
	p = TerminalCreateFeedback((char *) & (header->type), &len);
	GsmTaskSendTcpData(p, len);
	ProtocolDestroyMessage(p);
}

void HandleBroadcastTimes(ProtocolHeader *header, char *p) {
	int len, times;
	times = (*p++ - '0') * 10 + (*p - '0');
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
//    DisplayTask(p);
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




