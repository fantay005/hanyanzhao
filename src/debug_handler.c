#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rtc.h"
#include "second_datetime.h"
#include "led_lowlevel.h"
#include "softpwm_led.h"


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

#ifdef __LED_HUAIBEI__
static void __setSoftPWMLed(const char *p) {
	switch (p[4]) {
	case '0':
		SoftPWNLedSetColor(SoftPWNLedColorNULL);
		break;
	case '1':
		SoftPWNLedSetColor(SoftPWNLedColorRed);
		break;
	case '2':
		SoftPWNLedSetColor(SoftPWNLedColorOrange);
		break;
	case '3':
		SoftPWNLedSetColor(SoftPWNLedColorYellow);
		break;
	case '4':
		SoftPWNLedSetColor(SoftPWNLedColorBlue);
		break;
	default:
		break;
	}
}
#endif

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
#ifdef __LED_HUAIBEI__
	{ "SPWM", __setSoftPWMLed },
#endif
	{ "AT", __sendAtCommandToGSM },
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
