#include <string.h>
#include <stdio.h>
#include "atcmd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "zklib.h"

#ifndef AT_DBG
#define AT_DBG 0
#endif


#if AT_DBG
#  define dprintf(fmt, args...) printf(fmt, ##args)
#  define __atFree(p) do { vPortFree(p); } while (0)   // putchar('F');
static void *__atMalloc(size_t size) {
//	putchar('M');
	return pvPortMalloc(size);
}
#else
#  define dprintf(fmt, args ...) (void *)0)
#  define __atFree(p) vPortFree(p);
#  define __atMalloc(size) pvPortMalloc(size);
#endif

extern void ATCmdSendChar(unsigned char c);


static xQueueHandle __atCmdReplyQueue;

typedef struct {
	char *line;
	int len;
} ATCmdReplyInfo;

static inline void __atCmdDropReply(ATCmdReplyInfo *info) {
	__atFree(info);
}

static void __atCmdClearReply() {
	ATCmdReplyInfo *info;
	while (pdTRUE == xQueueReceive(__atCmdReplyQueue, &info, 0)) {
		__atCmdDropReply(info);
	}
}

static inline ATCmdReplyInfo *__atCmdGetReplyInfo(int timeoutTick) {
	ATCmdReplyInfo *info;
	if (pdFALSE == xQueueReceive(__atCmdReplyQueue, &info, timeoutTick)) {
		return NULL;
	}
	return info;
}


void ATCommandRuntimeInit(void) {
	__atCmdReplyQueue = xQueueCreate(5, sizeof(ATCmdReplyInfo *));		  //队列创建
}

int ATCommandGotLineFromIsr(const char *line, int len, portBASE_TYPE *pxHigherPriorityTaskWoken) {
	ATCmdReplyInfo *info;
	info = __atMalloc(len + ALIGNED_SIZEOF(ATCmdReplyInfo));
	if (info == NULL) {
		return pdFALSE;
	}
	info->line = ((char *)info) + ALIGNED_SIZEOF(ATCmdReplyInfo);
	info->len = len;
	memcpy(info->line, line, len);
	if (pdFALSE == xQueueSendFromISR(__atCmdReplyQueue, &info, pxHigherPriorityTaskWoken)) {
		__atCmdDropReply(info);
		return pdFALSE;
	}

	return pdTRUE;
}

ATCmdReplyInfo *__atCommand(const char *cmd, const char *prefix, int timeoutTick) {
	ATCmdReplyInfo *info;
	__atCmdClearReply();

//	printf("AT=>:%s\n", cmd);
	if (cmd != NULL) {
		while (*cmd) {
			ATCmdSendChar(*cmd++);
		}
	}

	if (prefix == NULL) {
		vTaskDelay(timeoutTick);
		return NULL;
	}

	while (1) {
		info = __atCmdGetReplyInfo(timeoutTick);
		if (info == NULL) {
			return NULL;
		}

//		printf("AT<=:%s\n", info->line);

		if (prefix == ATCMD_ANY_REPLY_PREFIX) {
			return info;
		}

		if (0 == strncmp(prefix, info->line, strlen(prefix))) {
			return info;
		}
		__atCmdDropReply(info);
	}
//	return NULL;
}


char *ATCommand(const char *cmd, const char *prefix, int timeoutTick) {
	ATCmdReplyInfo *info = __atCommand(cmd, prefix, timeoutTick);
	if (info) {
		return info->line;
	}
	return NULL;
}

void AtCommandDropReplyLine(char *line) {
	if (line != NULL) {
		__atCmdDropReply((ATCmdReplyInfo *)(line - ALIGNED_SIZEOF(ATCmdReplyInfo)));
	}
}

int ATCommandAndCheckReply(const char *cmd, const char *prefix, int timeoutTick) {
	ATCmdReplyInfo *info = __atCommand(cmd, prefix, timeoutTick);
	__atCmdDropReply(info);
	return (NULL != info);
}

int ATCommandAndCheckReplyUntilOK(const char *cmd, const char *prefix, int timeoutTick, int times) {
	while (times-- > 0) {
		if (ATCommandAndCheckReply(cmd, prefix, timeoutTick)) {
			return 1;
		}
	}
	return 0;
}

