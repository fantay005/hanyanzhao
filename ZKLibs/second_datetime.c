#include <stdbool.h>
#include <stdio.h>
#include "second_datetime.h"
#include "zklib.h"


#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400
#define SECONDS_PER_WEEK (60 * 60 * 24 * 7)
#define SECONDS_PER_COM_YEAR 31536000
#define SECONDS_PER_LEAP_YEAR 31622400
#define SECONDS_PER_FOUR_YEARS 126230400


static inline bool __isLeapYear(const DateTime *dateTime) {
	return (dateTime->year % 4) == 0;
}

//static const uint32_t __fourYearSecondsTable[] = {
//	0,
//	SECONDS_PER_LEAP_YEAR,
//	SECONDS_PER_LEAP_YEAR + SECONDS_PER_COM_YEAR * 1,
//	SECONDS_PER_LEAP_YEAR + SECONDS_PER_COM_YEAR * 2,
//};

static const uint32_t __fourYearSecondsTable[] = {
	0,
	SECONDS_PER_COM_YEAR * 1,
	SECONDS_PER_COM_YEAR * 2,
	SECONDS_PER_COM_YEAR * 3,
};

static  uint32_t __calYear(DateTime *dateTime, uint32_t second) {
	int i;

	dateTime->year = (second / SECONDS_PER_FOUR_YEARS) * 4;
	second = second % SECONDS_PER_FOUR_YEARS; // seconds left for 4 year;
	for (i = 0; i < ARRAY_MEMBER_NUMBER(__fourYearSecondsTable); ++i) {
		if (second < __fourYearSecondsTable[i]) {
			break;
		}
	}
	--i;
	dateTime->year += i;
	return second - __fourYearSecondsTable[i];
}



static  uint32_t __yearToSeconds(const DateTime *dateTime) {
	uint32_t  data;
	data = 126230400;
	printf("INT = %d.\r", data);
	data = (dateTime->year / 4);
  data *= 126230400;
	printf("Data = %d.\r", data);
	data += __fourYearSecondsTable[dateTime->year % 4];
	return data;
}


static const uint16_t __comYearMonthDaysNumAccTable[] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};
static const uint16_t __leapYearMonthDaysNumAccTable[] = {
	0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335
};

static  void __calMonthDate(DateTime *dateTime, int dayOffsetOfYear) {
	int i;
	const uint16_t *table;
	table = __isLeapYear(dateTime) ? __leapYearMonthDaysNumAccTable : __comYearMonthDaysNumAccTable;

	for (i = 1; i < 12; ++i) {
		if (dayOffsetOfYear < table[i]) {
			break;
		}
	}

	dateTime->month = i;
	dateTime->date = dayOffsetOfYear - table[i - 1] + 1;
}

static  uint32_t __monthDateToSeconds(const DateTime *dateTime) {
	const uint16_t *table;
	table = __isLeapYear(dateTime) ? __leapYearMonthDaysNumAccTable : __comYearMonthDaysNumAccTable;

	return ((table[dateTime->month - 1] + dateTime->date - 1) * SECONDS_PER_DAY);
}


static inline void __calWeekDay(DateTime *dateTime, uint32_t second) {
	int day;
	second = (second % SECONDS_PER_WEEK);
	second += (SECONDS_PER_DAY - 1);
	day = second / SECONDS_PER_DAY;
	day += 4; // 2000-1-1ÊÇÐÇÆÚÎå
	if (day > 6) {
		dateTime->day = day - 6;
	} else {
		dateTime->day = day + 1;
	}
}

static inline void __calTime(DateTime *dateTime, uint32_t second) {
	second = second % SECONDS_PER_DAY;
	dateTime->hour = second / SECONDS_PER_HOUR;
	second = second % SECONDS_PER_HOUR; // seconds left for one hour;
	dateTime->minute = second / SECONDS_PER_MINUTE;
	dateTime->second = second % SECONDS_PER_MINUTE;
}

static inline uint32_t __timeToSeconds(const DateTime *dateTime) {
	return dateTime->hour * SECONDS_PER_HOUR + dateTime->minute * SECONDS_PER_MINUTE + dateTime->second;
}

void SecondToDateTime(DateTime *dateTime, uint64_t second) {
	__calWeekDay(dateTime, second);
	second = __calYear(dateTime, second);
	__calMonthDate(dateTime, second / SECONDS_PER_DAY);
	__calTime(dateTime, second);
}

uint64_t DateTimeToSecond(const DateTime *dateTime) {
	uint32_t second;
	second = __yearToSeconds(dateTime);
	second += __monthDateToSeconds(dateTime);
	second += __timeToSeconds(dateTime);
	printf("Second is %d.\r", second);
	return second;
}

