#include "stm32f10x_iwdg.h"
#include "FreeRTOS.h"
#include "task.h"

static char __needReset = 0;
void WatchdogInit() {
	// 写入0x5555,用于允许狗狗寄存器写入功能
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	// 狗狗时钟分频,40K/256=156HZ(6.4ms)  可以分为 4，8，16，32，64，128，256
	IWDG_SetPrescaler(IWDG_Prescaler_256);
	//喂狗时间 1s=156 @IWDG_Prescaler_256.注意不能大于4096
	IWDG_SetReload(780);     //5秒
	IWDG_Enable();
}

void WatchdogStopFeed() {
	__needReset = 1;
}

void WatchdogFeed() {
	static unsigned int lastTick = 0;
	if (__needReset) {
		return;
	}
	if ((xTaskGetTickCount() - lastTick) > configTICK_RATE_HZ) {
		lastTick = xTaskGetTickCount();
		IWDG_ReloadCounter();
	}
}
