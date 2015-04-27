#ifndef __SECOND_DATE_H__
#define __SECOND_DATE_H__

#include <stdint.h>

/// \brief  描述时间的结构体.
typedef struct {
	/// \brief  年.
	/// 如果是2000年, 该值为0, 2001年该值为2001, 以此类推.
	uint8_t year;
	/// \brief  月.
	/// 取值范围为1-12.
	uint8_t month;
	/// \brief  日.
	/// 取值范围为1-31.
	uint8_t date;
	/// \brief  星期1-7.
	/// 取值范围为1-7.
	///uint8_t day;
	/// \brief  小时.
	/// 取值范围为0-23.
	uint8_t hour;
	/// \brief  分钟.
	/// 取值范围为0-59.
	uint8_t minute;
	/// \brief  秒.
	/// 取值范围为0-59.
	uint8_t second;
} DateTime;


/// \brief  把秒转换成时间.
/// \param  second[in]     从2000/1/1-00:00:00秒之后开始计时的秒数.
/// \param  dateTime[out]  存放转换好的时间.
void SecondToDateTime(DateTime *dateTime, uint32_t second);

/// \brief  把时间转换成秒.
/// \param  dateTime[in]  需要转换的时间.
/// \return 从2000/1/1-00:00:00秒之后开始计时的秒数.
uint32_t DateTimeToSecond(const DateTime *dateTime);

unsigned int __OffsetNumbOfDay(const DateTime *dateTime);

#endif
