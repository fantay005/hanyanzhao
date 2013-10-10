#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__


/// \brief  看门够初始化.
/// 初始化CPU片上看门狗, 5秒中复位.
void WatchdogInit(void);

/// \brief  停止喂看门够.
/// 在执行这个函数之后的一段时间系统将被看门狗服务.
void WatchdogResetSystem(void);

/// \brief  喂看门狗.
/// 该函数必须在最低优先级任务执行, 每个一秒钟喂看门狗.
void WatchdogFeed(void);

#endif
