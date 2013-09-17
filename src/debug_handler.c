#include <stdio.h>
#include <string.h>

typedef struct {
	const char *prefix;
	void (*func)(const char *);
} DebugHandlerMap;

static const DebugHandlerMap __handlerMaps[] = {
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
