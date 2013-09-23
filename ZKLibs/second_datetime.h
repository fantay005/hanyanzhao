#ifndef __SECOND_DATE_H__
#define __SECOND_DATE_H__

#include <stdint.h>

typedef struct {
	uint16_t year; //<! 年, 从1表示 2001年
	uint8_t month; //<! 月.
	uint8_t date; //<! 日.
	uint8_t day;  //<! 星期.
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
} DateTime;


/// \param second 从2000/1/1-00:00:00秒之后开始计时的秒数.
void SecondToDateTime(DateTime *dateTime, uint32_t second);
/// \return 从2000/1/1-00:00:00秒之后开始计时的秒数.
uint32_t DateTimeToSecond(const DateTime *dateTime);


#endif
