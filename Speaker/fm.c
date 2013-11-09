#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "stm32f10x_gpio.h"
#include "fm.h"
#include "soundcontrol.h"

#define DURATION_INIT_1 	60
#define DURATION_INIT_2	    60
#define DURATION_INIT_3 	60

#define DURATION_START_1	360
#define DURATION_START_2	360
#define DURATION_START_3	480

#define DURATION_STOP_1  	480
#define DURATION_STOP_2	    360
#define DURATION_STOP_3	    780

#define DURATION_HIGH		1000
#define DURATION_LOW		2000
#define POWER_SETTLING		66

typedef enum OPERA_MODE {
	READ = 1,
	WRITE = 2
} T_OPERA_MODE;

typedef enum ERROR_OP {
	Si_ERROR = 1,
	I2C_ERROR ,
	LOOP_EXP_ERROR ,
	OK
} T_ERROR_OP;

typedef enum POWER_UP_TYPE {
	FM_RECEIVER = 1,
	FM_TRNSMITTER = 2,
	AM_RECEIVER = 3
} T_POWER_UP_TYPE;

//typedef enum SEEK_MODE {
//	SEEKDOWN_HALT = 1,
//	SEEKDOWN_WRAP = 2,
//	SEEKUP_HALT = 3,
//	SEEKUP_WRAP = 4
//} T_SEEK_MODE;


#define WRITE_ADDR 0xC6
#define READ_ADDR 0xC7

#define RST_PIN			GPIO_Pin_5
#define SDIO_PIN		GPIO_Pin_11
#define SCLK_PIN		GPIO_Pin_10

#define RST_LOW		GPIO_ResetBits(GPIOB, RST_PIN)			
#define RST_HIGH	GPIO_SetBits(GPIOB, RST_PIN)

void inline __SDIO_DIR_OUT(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin =  SDIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}
#define SDIO_DIR_OUT  __SDIO_DIR_OUT()

void inline __RST_DIR_OUT(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = RST_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}
#define RST_DIR_OUT   __RST_DIR_OUT()

void inline __SCLK_DIR_OUT(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = SCLK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}
#define	SCLK_DIR_OUT   __SCLK_DIR_OUT()

void inline __SDIO_DIR_IN(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = SDIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}
#define SDIO_DIR_IN  __SDIO_DIR_IN()

#define SDIO_LOW	GPIO_ResetBits(GPIOB, SDIO_PIN)	
#define SDIO_HIGH	GPIO_SetBits(GPIOB, SDIO_PIN)	
#define READ_SDIO	GPIO_ReadInputDataBit(GPIOB,SDIO_PIN) 
#define SCLK_LOW	GPIO_ResetBits(GPIOB, SCLK_PIN)	
#define SCLK_HIGH	GPIO_SetBits(GPIOB, SCLK_PIN)

void inline __RST_PIN_INIT(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = RST_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, RST_PIN);
}
#define	RST_PIN_INIT  __RST_PIN_INIT()

void inline __SDIO_PIN_INIT(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = SDIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, SDIO_PIN);
}
#define	SDIO_PIN_INIT   __SDIO_PIN_INIT()

void inline  __SCLK_PIN_INIT(void) {
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = SCLK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, SCLK_PIN);
}
#define	SCLK_PIN_INIT  __SCLK_PIN_INIT()

#define DELAY(DURATION)		{vu16 i; for(i = 1; i <DURATION; i++){}}

static void __initGpio() {
	GPIO_InitTypeDef  GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = SCLK_PIN | SDIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, SCLK_PIN);

	GPIO_InitStructure.GPIO_Pin = RST_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, RST_PIN);
}

static  void ResetSi4731_2w(void) {
	__initGpio();
	SDIO_LOW;	
	SDIO_HIGH;
	RST_LOW;
	SCLK_HIGH;
	DELAY(DURATION_INIT_1);
	RST_HIGH;
	DELAY(DURATION_INIT_2);
	SDIO_HIGH;
	DELAY(DURATION_INIT_3);
}

static void __txByte(unsigned char byte) {
	int i;
	for (i = 7; i >= 0; i--) {                   //发送从机地址
		if ((byte >> i) & 0x01) {
			SDIO_HIGH;
		} else {
			SDIO_LOW;
		}
		DELAY(DURATION_LOW / 2);
		SCLK_HIGH;
		DELAY(DURATION_HIGH);
		SCLK_LOW;
		DELAY(DURATION_LOW / 2);
	}
}

static bool __waitAck() {
	unsigned char ret;
	SDIO_DIR_IN;
	DELAY(DURATION_LOW / 2);
	SCLK_HIGH;
	DELAY(DURATION_HIGH);
	ret = READ_SDIO;
	SCLK_LOW;
	DELAY(DURATION_LOW / 2);
	return ret == 0;
}

static u8 OperationSi4731_2w(T_OPERA_MODE operation, u8 *data, u8 numBytes) {
	uint8_t controlWord,  j, error = 0;
	int i;

	SCLK_HIGH;
	SDIO_HIGH;
	DELAY(DURATION_START_1);
	SDIO_LOW;
	DELAY(DURATION_START_2);
	SCLK_LOW;
	DELAY(DURATION_START_3);

	if (operation == READ) {                      //READ 说明主机准备接受模块发送数据
		controlWord = 0xC7;
	} else {                                      //WRITE 说明模块准备接受主机发送数据
		controlWord = 0xC6;
	}

	__txByte(controlWord);

	if (!__waitAck()) {
		goto STOP;
	}


	for (j = 0; j < numBytes; j++, data++) {
		if (operation == WRITE) {
			SDIO_DIR_OUT;
		} else {
			SDIO_DIR_IN;
		}
		for (i = 7; i >= 0; i--) {
			if (operation == WRITE)
				if ((*data >> i) & 0x01) {
					SDIO_HIGH;
				} else {
					SDIO_LOW;
				}
			DELAY(DURATION_LOW / 2);
			SCLK_HIGH;
			DELAY(DURATION_HIGH);
			if (operation == READ) {
				*data = (*data << 1) | READ_SDIO;    //地址data所指向的数值被赋予从端口督导的值
			}
			SCLK_LOW;
			DELAY(DURATION_LOW / 2);
		}
		if (operation == WRITE) {
			SDIO_DIR_IN;
		} else {
			SDIO_DIR_OUT;
			if (j == (numBytes - 1)) {
				SDIO_HIGH;
			} else {
				SDIO_LOW;
			}
		}
		DELAY(DURATION_LOW / 2);
		SCLK_HIGH;
		DELAY(DURATION_HIGH);
		if (operation == WRITE)
			if (READ_SDIO != 0) {
				error = 1;
				goto STOP;
			}
		SCLK_LOW;
		DELAY(DURATION_LOW / 2);
	}
STOP:
	SDIO_DIR_OUT;
	SDIO_LOW;
	DELAY(DURATION_STOP_1);
	SCLK_HIGH;
	DELAY(DURATION_STOP_2);
	SDIO_HIGH;
	DELAY(DURATION_STOP_3);
	return (error);
}

static T_ERROR_OP Si4731_Power_Down(void) {
	uint16_t loop_counter = 0;
	uint8_t Si4731_reg_data[32];
	uint8_t error_ind = 0;
	uint8_t Si4731_power_down[] = {0x11};

	error_ind = OperationSi4731_2w(WRITE, &(Si4731_power_down[0]), 1);
	if (error_ind) {
		return I2C_ERROR;
	}
	do {
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if (error_ind) {
			return I2C_ERROR;
		}
		loop_counter++;
	} while (((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));
	if (loop_counter >= 0xff) {
		return LOOP_EXP_ERROR;
	}
	return OK;
}

#if 0

static  T_ERROR_OP Si4731_Power_Up(T_POWER_UP_TYPE power_up_type) {
	uint8_t Si4731_power_up[] = {0x01, 0xC1, 0x05};

	switch (power_up_type) {
	case FM_RECEIVER: {
		Si4731_power_up[1] = 0xD0;
		Si4731_power_up[2] = 0x05;
		break;
	}
	case FM_TRNSMITTER: {
		Si4731_power_up[1] = 0xC2;
		Si4731_power_up[2] = 0x50;
		break;
	}
	case AM_RECEIVER: {
		Si4731_power_up[1] = 0xC1;
		Si4731_power_up[2] = 0x05;
		break;
	}
	}

	ResetSi4731_2w();
	vTaskDelay(configTICK_RATE_HZ / 5);
	if (__command(Si4731_power_up, 3)) {
		return OK;
	}
	return LOOP_EXP_ERROR;
}

#else

static  T_ERROR_OP Si4731_Power_Up(T_POWER_UP_TYPE power_up_type) {
	uint16_t loop_counter = 0;
	uint8_t Si4731_reg_data[32];
	uint8_t error_ind = 0;
	uint8_t Si4731_power_up[] = {0x01, 0xC1, 0x05};

	switch (power_up_type) {
	case FM_RECEIVER: {
		Si4731_power_up[1] = 0xD0;
		Si4731_power_up[2] = 0x05;
		break;
	}
	case FM_TRNSMITTER: {
		Si4731_power_up[1] = 0xC2;
		Si4731_power_up[2] = 0x50;
		break;
	}
	case AM_RECEIVER: {
		Si4731_power_up[1] = 0xC1;
		Si4731_power_up[2] = 0x05;
		break;
	}
	}

	ResetSi4731_2w();
	vTaskDelay(configTICK_RATE_HZ / 5);
	error_ind = OperationSi4731_2w(WRITE, &(Si4731_power_up[0]), 3);
	if (error_ind) {
		return I2C_ERROR;
	}
	vTaskDelay(configTICK_RATE_HZ / 2);;
	do {
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if (error_ind) {
			return I2C_ERROR;
		}
		loop_counter++;
	} while (((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff)); 
	if (loop_counter >= 0xff) {
		return LOOP_EXP_ERROR;
	}
	return OK;
}
#endif

static T_ERROR_OP Si4731_Set_Property_GPO_IEN(void) {
	uint16_t loop_counter = 0;
	uint8_t Si4731_reg_data[32];
	uint8_t error_ind = 0;
	uint8_t Si4731_set_property[] = {0x12, 0x00, 0x00, 0x01, 0x00, 0xCD};	//set STCIEN,CTSIEN

	error_ind = OperationSi4731_2w(WRITE, &(Si4731_set_property[0]), 6);
	if (error_ind) {
		return I2C_ERROR;
	}
	do {
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if (error_ind) {
			return I2C_ERROR;
		}
		loop_counter++;
	} while (((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));
	if (loop_counter >= 0xff) {
		return LOOP_EXP_ERROR;
	}
	return OK;
}


//static T_ERROR_OP Si4731_Get_INT_status(void)
//{
//	uint16_t loop_counter = 0;
//	uint8_t Si4731_reg_data[32];	
//	uint8_t error_ind = 0;
//	uint8_t Si4731_Get_INT_status[] = {0x14};	
//
// 	error_ind = OperationSi4731_2w(WRITE, &(Si4731_Get_INT_status[0]), 1);
//	if(error_ind)
//		return I2C_ERROR;
//	do
//	{	
//		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
//		if(error_ind)
//			return I2C_ERROR;	
//		loop_counter++;
//	}
//	while(((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));
//	if(loop_counter >= 0xff)
//		return LOOP_EXP_ERROR;	
//	return OK;
//}

//static T_ERROR_OP Si4731_Set_Property_FM_Seek_Band_Bottom(void) {
//	uint16_t loop_counter = 0;
//	uint8_t Si4731_reg_data[32];
//	uint8_t error_ind = 0;
//	uint8_t Si4731_set_property[] = {0x12, 0x00, 0x14, 0x00, 0x22, 0x2E};	//0x222E = 8750
//
//	error_ind = OperationSi4731_2w(WRITE, &(Si4731_set_property[0]), 6);
//	if (error_ind) {
//		return I2C_ERROR;
//	}
//
//	do {
//		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
//		if (error_ind) {
//			return I2C_ERROR;
//		}
//		loop_counter++;
//	} while (((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff)); 
//	if (loop_counter >= 0xff) {
//		return LOOP_EXP_ERROR;
//	}
//	return OK;
//}


//static T_ERROR_OP Si4731_Set_Property_FM_Seek_Space(void) {
//	uint16_t loop_counter = 0;
//	uint8_t Si4731_reg_data[32];
//	uint8_t error_ind = 0;
//	uint8_t Si4731_set_property[] = {0x12, 0x00, 0x14, 0x02, 0x00, 0x05};	//seek space = 0x0A = 10 = 100KHz
//
//	error_ind = OperationSi4731_2w(WRITE, &(Si4731_set_property[0]), 6);
//	if (error_ind) {
//		return I2C_ERROR;
//	}
//	do {
//		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
//		if (error_ind) {
//			return I2C_ERROR;
//		}
//		loop_counter++;
//	} while (((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));
//	if (loop_counter >= 0xff) {
//		return LOOP_EXP_ERROR;
//	}
//	return OK;
//}

static T_ERROR_OP Si4731_Wait_STC(void) {
	uint16_t loop_counter = 0, loop_counter_1 = 0;
	uint8_t Si4731_reg_data[32];
	uint8_t error_ind = 0;
	uint8_t Si4731_get_int_status[] = {0x14};	//读中断位

	do {
		error_ind = OperationSi4731_2w(WRITE, &(Si4731_get_int_status[0]), 1);

		if (error_ind) {
			return I2C_ERROR;
		}
		do {
			error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
			if (error_ind) {
				return I2C_ERROR;
			}
			loop_counter_1++;
		} while (((Si4731_reg_data[0] & 0x80) == 0) && (loop_counter_1 < 0xff));
		if (loop_counter_1 >= 0xff) {
			return LOOP_EXP_ERROR;
		}
		loop_counter_1 = 0;
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if (error_ind) {
			return I2C_ERROR;
		}
		loop_counter++;
	} while (((Si4731_reg_data[0] & 0x01) == 0) && (loop_counter < 0xfff));
	if (loop_counter == 0xfff) {
		return LOOP_EXP_ERROR;
	}
	return OK;
}


static T_ERROR_OP Si4731_FM_Tune_Freq(uint16_t channel_freq) {
	uint16_t loop_counter = 0;
	uint8_t Si4731_reg_data[32];
	uint8_t error_ind = 0;
	uint8_t Si4731_tune_freq[] = {0x20, 0x00, 0x27, 0xF6, 0x00};	//0x27F6=10230KHz

	Si4731_tune_freq[2] = (channel_freq & 0xff00) >> 8;
	Si4731_tune_freq[3] = (channel_freq & 0x00ff);
	//send CMD
	error_ind = OperationSi4731_2w(WRITE, &(Si4731_tune_freq[0]), 5);
	if (error_ind) {
		return I2C_ERROR;
	}
	do {
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if (error_ind) {
			return I2C_ERROR;
		}
		loop_counter++;
	} while (((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));
	if (loop_counter >= 0xff) {
		return LOOP_EXP_ERROR;
	}
	return OK;
}

static T_ERROR_OP Si4731_Set_Property_FM_DEEMPHASIS(void)
{
	u16 loop_counter = 0;
	u8 Si4731_reg_data[32];	
	u8 error_ind = 0;
	u8 Si4731_set_property[] = {0x12,0x00,0x11,0x00,0x00,0x01};	//FM deemphasis is 50us
        //0x12 设置特性值；0x1100 FM_DEEMPHASIS的属性； FM_DEEMPHASIS的Default值为75us(0x0002) ，50us(0x0001)
	//send CMD
 	error_ind = OperationSi4731_2w(WRITE, &(Si4731_set_property[0]), 6);
	if(error_ind)
		return I2C_ERROR;

	//wait CTS = 1(清除发送下一个指令）
	do
	{	
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if(error_ind)
			return I2C_ERROR;	
		loop_counter++;
	}
	while(((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));  //loop_counter limit should guarantee at least 300us
	
	if(loop_counter >= 0xff)
		return LOOP_EXP_ERROR;	

	return OK;

}


static T_ERROR_OP Si4731_Set_Property_FM_SNR_Threshold(void)
{
	u16 loop_counter = 0;
	u8 Si4731_reg_data[32];	
	u8 error_ind = 0;
	u8 Si4731_set_property[] = {0x12,0x00,0x14,0x03,0x00,0x3C};	//SNR threshold = 0x0003 = 3dB
        //0x1403为FM_SEEK_TUNE_SNR_THERSHOLD的属性；缺省值为0x0003=3db
	//send CMD
 	error_ind = OperationSi4731_2w(WRITE, &(Si4731_set_property[0]), 6);
	if(error_ind)
		return I2C_ERROR;

	//wait CTS = 1
	do
	{	
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if(error_ind)
			return I2C_ERROR;	
		loop_counter++;
	}
	while(((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));  //loop_counter limit should guarantee at least 300us
	
	if(loop_counter >= 0xff)
		return LOOP_EXP_ERROR;	
	return OK;

}


static T_ERROR_OP Si4731_Set_Property_FM_RSSI_Threshold(void)
{
	u16 loop_counter = 0;
	u8 Si4731_reg_data[32];	
	u8 error_ind = 0;
	u8 Si4731_set_property[] = {0x12,0x00,0x14,0x04,0x00,0x20};	//RSSI threshold = 0x0014 = 20dBuV
        //0x1404为FM_SEEK_TUNE_RSSI_TRESHOLD的属性；缺省值为0x0014=20dBuV
	//send CMD
 	error_ind = OperationSi4731_2w(WRITE, &(Si4731_set_property[0]), 6);
	if(error_ind)
		return I2C_ERROR;

	//wait CTS = 1
	do
	{	
		error_ind = OperationSi4731_2w(READ, &(Si4731_reg_data[0]), 1);
		if(error_ind)
			return I2C_ERROR;	
		loop_counter++;
	}
	while(((Si4731_reg_data[0]) != 0x80) && (loop_counter < 0xff));  //loop_counter limit should guarantee at least 300us
	
	if(loop_counter >= 0xff)
		return LOOP_EXP_ERROR;	

	return OK;

}


static T_ERROR_OP Si4731_Set_FM_Frequency(uint16_t channel_freq) {
	if (Si4731_FM_Tune_Freq(channel_freq) != OK) {
		return Si_ERROR;
	}
	if (Si4731_Wait_STC() != OK) {
		return Si_ERROR;
	}
	return OK;
}

void fmopen(int freq) {
	if ((freq < 875) || (freq > 1080)) {
		return;
	}
	freq = freq * 10;
	RST_PIN_INIT;
	SDIO_PIN_INIT;
	SCLK_PIN_INIT;
	RST_LOW;
	vTaskDelay(configTICK_RATE_HZ / 10);
	RST_HIGH;
	vTaskDelay(configTICK_RATE_HZ / 10);
	Si4731_Power_Down();
	vTaskDelay(configTICK_RATE_HZ / 5);
	Si4731_Power_Up(FM_RECEIVER);
	vTaskDelay(configTICK_RATE_HZ / 10);
	Si4731_Set_Property_GPO_IEN();
	vTaskDelay(configTICK_RATE_HZ / 10);
    Si4731_Set_Property_FM_DEEMPHASIS();
    vTaskDelay(configTICK_RATE_HZ / 10);
    Si4731_Set_Property_FM_SNR_Threshold();
    vTaskDelay(configTICK_RATE_HZ / 10);
    Si4731_Set_Property_FM_RSSI_Threshold();
	SoundControlSetChannel(SOUND_CONTROL_CHANNEL_FM, 1);
	vTaskDelay(configTICK_RATE_HZ / 10);
	Si4731_Set_FM_Frequency(freq);
}


