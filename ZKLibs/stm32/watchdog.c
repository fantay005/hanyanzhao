#include "stm32f10x_iwdg.h"
#include "FreeRTOS.h"
#include "task.h"

static char __needResetSystem = 0;

void WatchdogInit(void) {
	// Ð´Èë0x5555,ÓÃÓÚÔÊÐí¹·¹·¼Ä´æÆ÷Ð´Èë¹¦ÄÜ
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	// ¹·¹·Ê±ÖÓ·ÖÆµ,40K/256=156HZ(6.4ms)  ¿ÉÒÔ·ÖÎª 4£¬8£¬16£¬32£¬64£¬128£¬256
	IWDG_SetPrescaler(IWDG_Prescaler_256);
	//Î¹¹·Ê±¼ä 1s=156 @IWDG_Prescaler_256.×¢Òâ²»ÄÜ´óÓÚ4096
	IWDG_SetReload(780);     //5Ãë ¡//780
	IWDG_Enable();
}

void WatchdogResetSystem(void) {
	__needResetSystem = 1;
}

void WatchdogFeed(void) {
	static uint32_t lastTick = 0;
	if (!__needResetSystem) {
		uint32_t currentTick = xTaskGetTickCount();
		if ((currentTick - lastTick) > configTICK_RATE_HZ) {
			lastTick = currentTick;
			IWDG_ReloadCounter();
		}
	}
}
