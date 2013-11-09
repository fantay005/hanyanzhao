#ifndef __GSM_H__
#define __GSM_H__

#include <stdbool.h>

typedef struct {
	char serverIP[16];
	unsigned int serverPORT;
	bool isonTCP;
} GMSParameter;

bool GsmTaskSetGprsConnect(bool state);
bool GsmTaskSendSMS(const char *pdu, int len);
const char *GsmGetIMEI(void);

#endif

