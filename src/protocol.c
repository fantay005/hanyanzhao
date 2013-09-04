#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "stdio.h"
#include "protocol.h"
#include "string.h"
#include "xfs.h"

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
	ClassificReply =	0x39,

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

const char *ProtocolMessage(TypeChoose type, Classific class, const char *message, int *size) {
	int i;
	unsigned char sum = 0;
	unsigned char *p, *ret;
	int len = message == NULL ? 0 : strlen(message);

	*size = sizeof(ProtocolHeader) + len + sizeof(ProtocolPadder);
	ret = p = pvPortMalloc(*size);

	*p++ = '#';
	*p++ = 'H';
	*p++ = len >> 8;
	*p++ = len;
	*p++ = type;
	*p++ = class;
	*p++ = 0;
	*p++ = 0;
	*p++ = 0;
	*p++ = 0;
	if (p != NULL) {
		strcpy((char *)p, message);
	}

	p = ret;
	for (i = 0; i < len + sizeof(ProtocolHeader); ++i) {
		sum += *p++;
	}

	*p++ = HexToChar(sum >> 4);
	*p++ = HexToChar(sum);
	*p++ = 0x0D;
	*p = 0x0A;
	return (const char *)ret;
}

const char *ProtoclCreatLogin(int *size) {
	return ProtocolMessage(TermActive, Login, "863070018087881", size);
}

const char *ProtoclCreateHeartBeat(int *size) {
	return ProtocolMessage(TermActive, Heart, NULL, size);
}

const char *ProtocolCreateSmsReply(int *size) {
	return ProtocolMessage(TypeChooseReply, ClassificReply, "3100", size);
}

typedef void (*ProtocolHandleFunction)(unsigned char *p, int len);
typedef struct {
	unsigned char type;
	unsigned char class;
	ProtocolHandleFunction func;
} ProtocolHandleMap;

void HandleSendSMS(unsigned char *p, int len) {
	printf("HandleSendSMS\n");
	xfsSpeak(p, len, TYPE_UCS2);
//	const char *p = ProtocolCreateSmsReply(&len);

//	ProtocolDestroyMessage(p);

	return;
}

void ProtocolHandler(unsigned char *p) {
//	if (strncmp(p, "#H", 2) != 0) return;
	int i;
	const static ProtocolHandleMap map[] = {
		//	{'1', '1', HandleLogin},
		//	{'1', '2', HandleHeartBeat},
		//	{'2', '0', HandleSettingUser},
		//	{'2', '1', HandleRemoveUser},
		//	{'2', '2', HandleDeadTime},
		//	{'2', '3', HandleVoiceType},
		//	{'2', '4', HandleVolumeSetting},
		//	{'2', '5', HandleBroadcastTimes},
		{'2', '6', HandleSendSMS},
		//	{'2', '7', HandleRestart},
		//	{'2', '8', HandleRecoverFactory},
		//	{'3', '1', HandleBasicParameter},
		//	{'3', '2', HandleCoordinate},
		//	{'4', '1', HandleRecordMP3},
		//	{'4', '2', HandleSMSPromptSound},
		//	{'4', '3', HandleRecordPromptSound},
		//	{'4', '4', HandleMP3Music},
		//	{'4', '5', HandleLongSMS},
	};
	ProtocolHeader *header = (ProtocolHeader *)p;
	int len = (header->lenH << 8) + header->lenL;
	printf("sizeof(ProtocolHeader)=%d\n", sizeof(ProtocolHeader));
	printf("Protocol:\nlen=%d\n", len);
	printf("type=%c\n", header->type);
	printf("class=%c\n", header->class);
	printf("content=%s", p + sizeof(ProtocolHeader));

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if ((map[i].type == header->type) && (map[i].class == header->class)) {
			map[i].func(p + sizeof(ProtocolHeader), len);
			break;
		}
	}
}
#if 0
void HandleLogin(unsigned char *p) {
}

void HandleHeartBeat(unsigned char *p) {
}

void HandleSettingUser(unsigned char *p) {
}

void HandleRemoveUser(unsigned char *p) {
}

void HandleDeadTime(unsigned char *p) {
}

void HandleVoiceType(unsigned char *p) {
}

void HandleVolumeSetting(unsigned char *p) {
}

void HandleBroadcastTimes(unsigned char *p) {
}
#endif

#if 0
void HandleRestart(unsigned char *p) {
}

void HandleRecoverFactory(unsigned char *p) {
}

void HandleBasicParameter(unsigned char *p) {
}

void HandleCoordinate(unsigned char *p) {
}

void HandleRecordMP3(unsigned char *p) {
}

void HandleSMSPromptSound(unsigned char *p) {
}

void HandleRecordPromptSound(unsigned char *p) {
}

void HandleMP3Music(unsigned char *p) {
}

void HandleLongSMS(unsigned char *p) {
}
#endif



