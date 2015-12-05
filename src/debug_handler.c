#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rtc.h"
#include "gsm.h"
#include "norflash.h"
#include "protocol.h"

void __sendAtCommandToGSM(const char *p) {
	extern int GsmTaskSendAtCommand(const char * atcmd);
	printf("SendAtCommandToGSM: ");
	if (GsmTaskSendAtCommand(p)) {
		printf("Succeed\n");
	} else {
		printf("Failed\n");
	}
}

static void __setGatewayParam(const char *p) {
	GMSParameter g;

	sscanf(p, "%*[^,]%*c%[^,]", g.GWAddr);
	sscanf(p, "%*[^,]%*c%*[^,]%*c%[^,]", g.serverIP);
	sscanf(p, "%*[^,]%*c%*[^,]%*c%*[^,]%*c%d", &(g.serverPORT));

	NorFlashWrite(NORFLASH_MANAGEM_ADDR, (const short *)&g, (sizeof(GMSParameter) + 1) / 2);
}

static void __QueryParamInfor(const char *p){
	
}

static void __erasureFlashChip(const char *p){
	NorFlashEraseChip();
}

typedef struct {
	const char *prefix;
	void (*func)(const char *);
} DebugHandlerMap;

static const DebugHandlerMap __handlerMaps[] = {
	{ "AT", __sendAtCommandToGSM },
  { "WG", __setGatewayParam },
  { "ER", __erasureFlashChip},
	{ "E",  __QueryParamInfor},
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
