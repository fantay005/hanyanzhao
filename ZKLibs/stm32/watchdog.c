#include "stm32f10x_iwdg.h"
#include "FreeRTOS.h"
#include "task.h"

static char __needResetSystem = 0;

/// \brief  看门够初始化.
/// 初始化CPU片上看门狗, 5秒中复位.
void WatchdogInit(void) {
	// 写入0x5555,用于允许狗狗寄存器写入功能
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	// 狗狗时钟分频,40K/256=156HZ(6.4ms)  可以分为 4，8，16，32，64，128，256
	IWDG_SetPrescaler(IWDG_Prescaler_256);
	//喂狗时间 1s=156 @IWDG_Prescaler_256.注意不能大于4096
	IWDG_SetReload(780);     //5秒
	IWDG_Enable();
}

/// \brief  停止喂看门够.
/// 在执行这个函数之后的一段时间系统将被看门狗服务.
void WatchdogResetSystem(void) {
	__needResetSystem = 1;
}

/// \brief  喂看门狗.
/// 该函数必须在最低优先级任务执行, 每个一秒钟喂看门狗.
void WatchdogFeed() {
	static uint32_t lastTick = 0;
	if (!__needResetSystem) {
		uint32_t currentTick = xTaskGetTickCount();
		if ((currentTick - lastTick) > configTICK_RATE_HZ) {
			lastTick = currentTick;
			IWDG_ReloadCounter();
		}
	}
}
