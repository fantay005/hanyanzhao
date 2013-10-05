#ifndef __UNICODE2GBK_H__
#define __UNICODE2GBK_H__

#include <stdint.h>
#include "FreeRTOS.h"

uint8_t *Unicode2GBK(const uint8_t *unicode, int len);
static inline void Unicode2GBKDestroy(uint8_t *p) {
	vPortFree(p);
}

#endif
