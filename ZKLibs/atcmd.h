#ifndef __ATCMD_H__
#define __ATCMD_H__

#include <stdbool.h>
#include "FreeRTOS.h"


#define ATCMD_ANY_REPLY_PREFIX  ((const char *)0xFFFFFFFF)
void ATCommandRuntimeInit(void);
char *ATCommand(const char *cmd, const char *prefix, int timeoutTick);
void AtCommandDropReplyLine(char *line);
bool ATCommandAndCheckReply(const char *cmd, const char *prefix, int timeoutTick);
bool ATCommandAndCheckReplyUntilOK(const char *cmd, const char *prefix, int timeoutTick, int times);
bool ATCommandGotLineFromIsr(const char *line, unsigned char len, portBASE_TYPE *pxHigherPriorityTaskWoken);

#endif
