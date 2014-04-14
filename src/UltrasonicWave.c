#include "UltrasonicWave.h"
#include "stm32f10x.h"
#include "stm32f10x_GPIO.h"
#include "stm32f10x_RCC.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "FreeRtos.h"
#include "task.h"
#include "queue.h"
#include <stdbool.h>
 
#if defined(HEALTH)
	#define GPIOx		GPIOA	   
	#define GPIOy		GPIOF	  
	#define PINx_Echo	GPIO_Pin_1	 //ECHO  
	#define PINy_Trig	GPIO_Pin_10	 //TRIG 	
#else
	#define GPIOx		GPIOA	   
	#define GPIOy		GPIOB		  
	#define PINx_Echo	GPIO_Pin_1	 //ECHO          
	#define PINy_Trig	GPIO_Pin_11	 //TRIG  
#endif

#define TIMx			TIM2
#define TIMx_IRQHandler	TIM2_IRQHandler
#define TIMx_IRQn		TIM2_IRQn

#define FOSC				72000000UL
#define USPS				 1000000UL
#define SAMPLE_RATE 		  100000UL	//100kHz
#define TASK_STACK_SIZE 	256

typedef struct{
	uint32_t type;
	union{
		uint32_t v;
		void *p;
	}dat;
}qCmd_t;

static xQueueHandle __qCmd	= 0;

#define CALC(val)			((val)*(USPS/SAMPLE_RATE)/2)
#define __SendData(dat)		//Dev_SendIntData(DEV_HEIGHT,(dat), IN_TASK)

static
void __delay(int Time)    
{
   unsigned char i;
   for ( ; Time>0; Time--)
     for ( i = 0; i < 72; i++ );
}

static
void __Tim2_Init(void)
{
	/*配置定时器中断*/{
    NVIC_InitTypeDef NVIC_InitStructure; 
	
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);  													
    NVIC_InitStructure.NVIC_IRQChannel = TIMx_IRQn;	  
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;	
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	}
	/*配置定时器*/{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 , ENABLE);
    TIM_DeInit(TIMx);
    TIM_TimeBaseStructure.TIM_Period=65535;							/* 自动重装载寄存器周期的值(计数值) */
    TIM_TimeBaseStructure.TIM_Prescaler= (FOSC/SAMPLE_RATE - 1);	/* 调整预分频控制计数频率 */
    TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 		    /* 采样分频 */
    TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up;      	/* 向上计数模式 */
    TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);
    TIM_ClearFlag(TIMx, TIM_FLAG_Update);	
	}
	/*配置通道2为PWM输入模式*/{
	TIM_ICInitTypeDef  TIM_ICInitStructure;
	
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x0;
	TIM_PWMIConfig(TIMx, &TIM_ICInitStructure);
	
	TIM_SelectInputTrigger(TIMx, TIM_TS_TI2FP2);
	TIM_SelectSlaveMode(TIMx, TIM_SlaveMode_Reset);
	TIM_SelectMasterSlaveMode(TIMx, TIM_MasterSlaveMode_Enable);
	TIM_ITConfig(TIMx, TIM_IT_CC2, ENABLE);
	}
	TIM_Cmd(TIMx, ENABLE);
}

static void __task(void *nouse) {
	static bool run = false;
	static bool sequence = 5;

	qCmd_t qcmd;
	__qCmd = xQueueCreate(5, sizeof(qCmd_t));
	__Tim2_Init();							//开启中断后会用到__qCmd
	while(1) {
		GPIO_SetBits(GPIOy,PINy_Trig);	//送>10US的高电平
		__delay(20);					//延时20US
		GPIO_ResetBits(GPIOy,PINy_Trig);
		vTaskDelay(configTICK_RATE_HZ);
	}
}

void UltrasonicWave_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;  
	GPIO_InitStructure.GPIO_Pin = PINy_Trig; 				   //PC8接TRIG
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		   //设为推挽输出模式
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		   
	GPIO_Init(GPIOy, &GPIO_InitStructure);					   //初始化外设GPIO 

	GPIO_InitStructure.GPIO_Pin = PINx_Echo;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOx, &GPIO_InitStructure);	
    xTaskCreate(__task, (signed portCHAR *)"Height", 
                TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, NULL);

}
static unsigned int Value = 0;

void TIMx_IRQHandler(void)
{
	uint16_t IC1Value;
	if ( TIM_GetITStatus(TIMx , TIM_IT_Update) != RESET ){	
		TIM_ClearITPendingBit(TIMx , TIM_IT_Update); 
	}else if( TIM_GetITStatus(TIMx, TIM_IT_CC2) != RESET ){
		TIM_ClearITPendingBit(TIMx, TIM_IT_CC2);
		IC1Value = TIM_GetCapture1(TIMx);
    Value = CALC(IC1Value);
	}	 	
}

unsigned int measure(void){
	return Value;
}

