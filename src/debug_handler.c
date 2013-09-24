#include <stdio.h>
#include <string.h>
#include "rtc.h"
#include "second_datetime.h"


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

typedef struct {
	const char *prefix;
	void (*func)(const char *);
} DebugHandlerMap;

static const DebugHandlerMap __handlerMaps[] = {
	{ "ST", __setRtcTime },
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
