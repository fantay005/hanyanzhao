#ifndef __RTC_H__
#define __RTC_H__

#include <stdint.h>
#include <stdbool.h>
#include "stm32f10x_rtc.h"

void RtcInit(void);
void RtcSetTime(uint32_t seconds);
bool RtcIsSecondInterruptOccured(void);
static inline uint32_t RtcGetTime() {
	return RTC_GetCounter();
}

#endif
