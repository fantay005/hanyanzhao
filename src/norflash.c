#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_fsmc.h"

static void initHardware() {
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  FSMC_NORSRAMTimingInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	/*-- GPIO Configuration ------------------------------------------------------*/
	/* NOR Data lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 |
								  GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
								  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 |
								  GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOR Address lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |   //A0,A1,A2,A3,A4,A5,
								  GPIO_Pin_4 | GPIO_Pin_5 |
								  GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;//A6,A7,A8,A9
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |    //A10,A11,A12,A13,A14,A15,
								  GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;  //A16,A17,A18
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	//GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;  //A19,A20,   A21,A22
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;  //A19,A20,   A21  地址线没用那么大  wang2012-6-14
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOE and NWE configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NE2 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	/*-- FSMC Configuration ----------------------------------------------------*/
	FSMC_NORSRAMTimingInitStructure.FSMC_AddressSetupTime = 0x05;
	FSMC_NORSRAMTimingInitStructure.FSMC_AddressHoldTime = 0x00;
	FSMC_NORSRAMTimingInitStructure.FSMC_DataSetupTime = 0x07;
	FSMC_NORSRAMTimingInitStructure.FSMC_BusTurnAroundDuration = 0x00;
	FSMC_NORSRAMTimingInitStructure.FSMC_CLKDivision = 0x00;
	FSMC_NORSRAMTimingInitStructure.FSMC_DataLatency = 0x00;
	FSMC_NORSRAMTimingInitStructure.FSMC_AccessMode = FSMC_AccessMode_B;

	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM2;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_NOR;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

	/* Enable FSMC Bank1_NOR Bank */
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM2, ENABLE);

}
#if 0
void factoryinit(void) {
	int  i;
	int temp_buff[130];
	int prompt_buff[2];
	long temp_addr;
	FSMC_NOR_EraseBlock(0x3f0000);
	for (i = 0x2b; i < 0x3f; i++) {
		temp_addr = 0x10000 * i;
		FSMC_NOR_EraseBlock(temp_addr);
	}
	prompt_buff[0] = 0x0000;
	prompt_buff[1] = 0x0000;
	temp_addr = 0x3f4000;
	FSMC_NOR_WriteBuffer(temp_buff, temp_addr, 2);
	temp_buff[0] = 0xff55;  //这组系统参数正在使用
	temp_buff[1] = 5;  //停顿时间05
	temp_buff[2] = 3;  //次数03
	temp_buff[3] = 0x3338;  //男女生和音量
	temp_buff[4] = 0xFFFF;
	for (i = 5; i < 100; i++) {
		temp_buff[i] = 0xFFFF;
	}

	//存储IP地址
	temp_buff[100] = url_len - 25;
	temp_buff[101] = 0x787a;
	temp_buff[102] = 0x2e61;
	temp_buff[103] = 0x686e;
	temp_buff[104] = 0x772e;
	temp_buff[105] = 0x676f;
	temp_buff[106] = 0x762e;
	temp_buff[107] = 0x636e;
	temp_buff[108] = 0xFFFF;
	temp_buff[109] = 0xFFFF;

	//存储IP地址
	temp_buff[110] = 0x000E;
	temp_buff[111] = 0x3232;
	temp_buff[112] = 0x312e;
	temp_buff[113] = 0x3133;
	temp_buff[114] = 0x302e;
	temp_buff[115] = 0x3132;
	temp_buff[116] = 0x392e;
	temp_buff[117] = 0x3732;
	temp_buff[118] = 0x3535;
	temp_buff[119] = 0x3535;

	//存储IMEI
	for (i = 0; i < 15; i++) {
		if (i % 2 == 0) {
			temp_buff[120 + i / 2] = imei_buff[i];
			temp_buff[120 + i / 2] = temp_buff[120 + i / 2] << 8;
		}        else {
			temp_buff[120 + i / 2] = temp_buff[120 + i / 2] | imei_buff[i];
		}
	}

	temp_buff[128] = 0xFFFF;
	temp_buff[129] = 0xFFFF;


	temp_addr = 0x3f0000;   //存放系统参数的首地址
	FSMC_NOR_WriteBuffer(temp_buff, temp_addr, 130);


	for (i = 0; i < 7; i++) {
		sms_yinliang[i] = temp_buff[3];      //语音模块音量
		sms_yusu[i] = '4';          //语音模块语速
		sms_nannv[i] = temp_buff[3] >> 8;       //男女声
		sms_voice_flag[i] = 0;
		sms_voice_buff_flge[i] = 0;
		sms_voice_cishu_flge[i] = temp_buff[2];
	}

	sms_xunhuan = 0;
	sms_voice_buff_flge[7] = 0;
	sms_voice_buff_flge[8] = 0;
	sms_yanshi_sheding = temp_buff[1];      //一个信息播报时的延时设定值    5s
	sms_voice_mang = 0;
	sms_yinliang_old = temp_buff[3];
	sms_yusu_old = '4';
	sms_nannv_old = temp_buff[3] >> 8;

	for (i = 0; i < 6; i++) {
		sms_cdx_tou[i] = 0;
	}






	void vNorFlash(void * parameter) {
		initHardware();

	}
#endif
