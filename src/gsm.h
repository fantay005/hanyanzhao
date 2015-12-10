#ifndef __GSM_H__
#define __GSM_H__

#include <stdbool.h>

#define __MODEL_DEBUG__   1

typedef struct {
	unsigned char GWAddr[10];
	char serverIP[16];
	unsigned int serverPORT;
} GMSParameter;

bool GsmTaskSendSMS(const char *pdu, int len);
bool GsmTaskSendTcpData(const char *p, unsigned char len);

#endif

