#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rtc.h"
#include "second_datetime.h"
#include "led_lowlevel.h"


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

typedef struct {
	const char *prefix;
	void (*func)(const char *);
} DebugHandlerMap;

static const DebugHandlerMap __handlerMaps[] = {
	{ "ST", __setRtcTime },
	{ "SSB", __setScanBuffer},
	{ NULL, NULL },
};

void DebugHandler(const char *msg) {
	const DebugHandlerMap *map;

//	printf("DebugHandler: %s\n", msg);

	for (map = __handlerMaps; map->prefix != NULL; ++map) {
		if (0 == strncmp(map->prefix, msg, strlen(map->prefix))) {
			map->func(msg);
			return;
		}
	}
//	printf("DebugHandler: Can not handler\n");
}
