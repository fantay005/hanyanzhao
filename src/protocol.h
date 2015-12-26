#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "gsm.h"
#include "stm32f10x_gpio.h"

typedef struct {
	unsigned char header;
	unsigned char addr[10];
	unsigned char contr[2];
	unsigned char lenth[2];
} ProtocolHead;

void GPRSProtocolHandler(ProtocolHead *head, char *p);
void __handleInternalProtocol(ProtocolHead *head, char *p);


#endif
