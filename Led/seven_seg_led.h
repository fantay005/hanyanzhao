#ifndef __SEVEN_SEG_LEG_HH__
#define __SEVEN_SEG_LEG_HH__

#include <stdbool.h>
#include <stdint.h>

/// \brief  数码管的数量.
#define SEVEN_SEG_LED_NUM 20

/// \brief  初始化7段数码显示.
void SevenSegLedInit(void);

/// \brief  当需要关闭某个数码管时, SevenSegLedSetContent的what参数传递这个值.
#define SEVEN_SEG_LED_OFF 0xFF

/// \brief  设置7段数码显示的内容.
/// \param  index   数码管的编号, 可以是{ 0 - (SEVEN_SEG_LED_NUM-1) }.
/// \param  what    需要显示的内如, 取值范围是{ 0-9, SEVEN_SEG_LED_OFF }.
/// \return true    成功,
/// \return false   失败.
bool SevenSegLedSetContent(unsigned int index, uint8_t what);

/// \brief  显示已设置的内容.
void SevenSegLedDisplay(void);

#endif
