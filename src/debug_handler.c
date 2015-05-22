#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rtc.h"
#include "second_datetime.h"
#include "led_lowlevel.h"
#include "softpwm_led.h"
#include "gsm.h"
#include "norflash.h"


static void __setRtcTime(const char *p) {
	DateTime dateTime;
	p += 2;
	printf("SetTime:%s", p);
	dateTime.year = (p[0] - '0') * 10 + (p[1] - '0');
	dateTime.month = (p[2] - '0') * 10 + (p[3] - '0');
	dateTime.date = (p[4] - '0') * 10 + (p[5] - '0');
	dateTime.hour = (p[6] - '0') * 10 + (p[7] - '0');
	dateTime.minute = (p[8] - '0') * 10 + (p[9] - '0');
	if (p[10] != 0 && p[11] != 0) {
		dateTime.second = (p[10] - '0') * 10 + (p[11] - '0');
	} else {
		dateTime.second = 0;
	}
	RtcSetTime(DateTimeToSecond(&dateTime));
}

#ifdef __LED__
void __setScanBuffer(const char *p) {
	int mux = 0;
	int x = 0;
	unsigned char dat = 0x00;
	char *tmp;

	mux = strtol(&p[3], &tmp, 10);
	if ((tmp == NULL) || (*tmp != ','))	{
		printf("error format 1\n");
		return;
	}


	x = strtol(&tmp[1], &tmp, 10);
	if ((tmp == NULL) || (*tmp != ','))	{
		printf("error format 2\n");
		return;
	}

	dat = strtol(&tmp[1], &tmp, 16);
	printf("LedScanSetScanBuffer(%d, %d, %02X):", mux, x, dat);
	if (LedScanSetScanBuffer(mux, x, dat)) {
		printf("Succeed\n");
	} else {
		printf("Failed\n");
	}
}


void __setDisplayBuffer(const char *p) {
	int x = 0;
	int y = 0;
	unsigned char dat = 0x00;
	char *tmp;

	x = strtol(&p[3], &tmp, 10);
	if ((tmp == NULL) || (*tmp != ','))	{
		printf("error format 1\n");
		return;
	}


	y = strtol(&tmp[1], &tmp, 10);
	if ((tmp == NULL) || (*tmp != ','))	{
		printf("error format 2\n");
		return;
	}

	dat = strtol(&tmp[1], &tmp, 16);
	printf("LedDisplaySetPixel(%d, %d, %d):", x, y, dat ? 1 : 0);
	if (LedDisplaySetPixel(x, y, dat)) {
		LedDisplayToScan(x, y, x, y);
		printf("Succeed\n");
	} else {
		printf("Failed\n");
	}
}
#endif

void __sendAtCommandToGSM(const char *p) {
	extern int GsmTaskSendAtCommand(const char * atcmd);
	printf("SendAtCommandToGSM: ");
	if (GsmTaskSendAtCommand(p)) {
		printf("Succeed\n");
	} else {
		printf("Failed\n");
	}
}

static void __setShuncom(const char *p) {
	char buf1[4], buf2[8], buf3[2], buf4, buf5, count;
	extern void Shuncom_Config_Param(char addr[4], char name[8], char id[2], char fre, char flag, char type);
	count = strlen(p);
	if(count == 24){
		strncpy(buf1, (p + 4), 4);
		strncpy(buf2, (p + 9), 8);
		strncpy(buf3, (p + 18), 2);
		buf4 = *(p + 21);
		buf5 = *(p + 23);
	}
	Shuncom_Config_Param(buf1, buf2, buf3, buf4, buf5, count);
}

static void __setGatewayParam(const char *p) {
	GMSParameter g;
	memset(&g, 0, sizeof(GMSParameter));
	sscanf(p, "%*[^,]%*c%[^,]", g.GWAddr);
	sscanf(p, "%*[^,]%*c%*[^,]%*c%[^,]", g.serverIP);
	sscanf(p, "%*[^,]%*c%*[^,]%*c%*[^,]%*c%d", &(g.serverPORT));
//	g.serverPORT = atoi(msg);

	NorFlashWrite(NORFLASH_MANAGEM_ADDR, (const short *)&g, sizeof(GMSParameter));
	
}

static void __erasureFlashChip(const char *p){
	NorFlashEraseChip();
}


typedef struct {
	const char *prefix;
	void (*func)(const char *);
} DebugHandlerMap;

static const DebugHandlerMap __handlerMaps[] = {
	{ "ST", __setRtcTime },
#ifdef __LED__
	{ "SSB", __setScanBuffer},
	{ "SDB", __setDisplayBuffer },
#endif
	{ "CFG", __setShuncom },
	{ "AT", __sendAtCommandToGSM },
  { "WG", __setGatewayParam },
  { "ER", __erasureFlashChip},
	{ NULL, NULL },
};

void DebugHandler(const char *msg) {
	const DebugHandlerMap *map;

	printf("DebugHandler: %s\n", msg);

	for (map = __handlerMaps; map->prefix != NULL; ++map) {
		if (0 == strncmp(map->prefix, msg, strlen(map->prefix))) {
			map->func(msg);
			return;
		}
	}
	printf("DebugHandler: Can not handler\n");
}
