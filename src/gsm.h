#ifndef __GSM_H__
#define __GSM_H__

#include <stdbool.h>

typedef struct {
	char serverIP[16];
	unsigned int serverPORT;
	bool isonTCP;
	bool isonQUIET;
	char time[4];
} GMSParameter;

bool GsmTaskSetParameter(const char *dat, int len);
bool GsmTaskSendSMS(const char *pdu, int len);
const char *GsmGetIMEI(void);

#endif

