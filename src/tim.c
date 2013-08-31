#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x_tim.h"

vu16 CCR1_Val = 8192;
vu16 CCR2_Val = 8192;
vu16 CCR3_Val = 8192;
vu16 CCR4_Val = 8192;

static void initHardware() {

	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;

	TIM_TimeBaseStructure.TIM_Period = 60000;//这个值越小，中断时间越短,大约10//设置了在下一个更新事件装入活动的自动重装载寄存器周期的值。它的取值必须在 0x0000 和0xFFFF之间.
	//来设置自动装入的值
	TIM_TimeBaseStructure.TIM_Prescaler = 35999;//TIM_Prescaler设置了用来作为 TIMx 时钟频率除数的预分频值。它的取值必须在 0x0000 和0xFFFF 之间  计算方法：CK_INT/(TIM_Perscaler+1)
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;//TIM_ClockDivision 设置了时钟分割。
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;//TIM_CounterMode 选择了计数器模式,选择向上计数模式；
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	// Prescaler configuration
	TIM_PrescalerConfig(TIM2, 35999, TIM_PSCReloadMode_Immediate);

	// Output Compare Timing Mode configuration: Channel1
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = CCR1_Val;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	TIM_OC1Init(TIM2, &TIM_OCInitStructure);

	//TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);

	// TIM IT enable
	TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);
	TIM_Cmd(TIM2, DISABLE);

	//TIM3==========================================
	TIM_TimeBaseStructure.TIM_Period = 20000;
	TIM_TimeBaseStructure.TIM_Prescaler = 35999;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	// Prescaler configuration
	TIM_PrescalerConfig(TIM3, 35999, TIM_PSCReloadMode_Immediate);

	// Output Compare Timing Mode configuration: Channel1
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = CCR1_Val;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;

	TIM_OC1Init(TIM3, &TIM_OCInitStructure);

	//TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Disable);
	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);

	// TIM IT enable
	TIM_ITConfig(TIM3, TIM_IT_CC1, ENABLE);
	TIM_Cmd(TIM3, DISABLE);
}

void vTim(void *parameter) {
	initHardware();

}
