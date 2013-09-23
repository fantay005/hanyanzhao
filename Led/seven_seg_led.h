#ifndef __SEVEN_SEG_LEG_HH__
#define __SEVEN_SEG_LEG_HH__

#include <stdbool.h>

#define SEVEN_SEG_LED_NUM 20

/// 初始化7段数码显示.
void SevenSegLedInit(void);

/// 设置7段数码显示的内容.
/// \param index 数码管的编号, 可以是0 -- (SEVEN_SEG_LED_NUM-1).
/// \param waht 需要显示的内如.
/// \return true 成功,
/// \return false 失败.
bool SevenSegLedSetContent(unsigned int index, char what);

/// 显示已设置的内容.
void SevenSegLedDisplay(void);

#endif
