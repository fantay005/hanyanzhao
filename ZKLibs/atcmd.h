#ifndef __ATCMD_H__
#define __ATCMD_H__

#include "FreeRTOS.h"

#define ATCMD_ANY_REPLY_PREFIX  ((const char *)0xFFFFFFFF)

void ATCommandRuntimeInit(void);
char *ATCommand(const char *cmd, const char *prefix, int timeoutTick);
void AtCommandDropReplyLine(char *line);
int ATCommandAndCheckReply(const char *cmd, const char *prefix, int timeoutTick);
int ATCommandAndCheckReplyUntilOK(const char *cmd, const char *prefix, int timeoutTick, int times);
int ATCommandGotLineFromIsr(const char *line, int len, portBASE_TYPE *pxHigherPriorityTaskWoken);

#endif
