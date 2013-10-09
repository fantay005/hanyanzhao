#include <string.h>
#include "led_lowlevel.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_fsmc.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "font_dot_array.h"
#include "zklib.h"


#if !defined(LED_DRIVER_LEVEL)
#  error "LED_DRIVER_LEVEL MUST be defined"
#elif (LED_DRIVER_LEVEL!=0) && (LED_DRIVER_LEVEL!=1)
#  error "LED_DRIVER_LEVEL MUST be 0 or 1"
#endif

#if !defined(LED_SCAN_MUX)
#  error "LED_SCAN_MUX MUST be defined"
#elif (LED_SCAN_MUX!=2) && (LED_SCAN_MUX!=4) && (LED_SCAN_MUX!=8) && (LED_SCAN_MUX!=16)
#  error "LED_DRIVER_LEVEL MUST be 2,4,8 or 16"
#endif

#if !defined(LED_SCAN_LENGTH)
#  error "LED_SCAN_LENGTH MUST be defined"
#elif (LED_SCAN_LENGTH%8 != 0)
#  error "LED_SCAN_LENGTH%8 MUST be equal to 0"
#endif

#if !defined(LED_DOT_HEIGHT)
#  error "LED_DOT_HEIGHT MUST be defined"
#elif (LED_DOT_HEIGHT%16 != 0)
#  error "LED_DOT_HEIGHT%16 MUST be equal to 0"
#endif


#if !defined(LED_DOT_WIDTH)
#  error "LED_DOT_WIDTH MUST be defined"
#elif (LED_DOT_WIDTH%8 != 0)
#  error "LED_DOT_HEIGHT%8 MUST be equal to 0"
#endif



static unsigned char __scanLine = 0;
static unsigned char __scanBuffer[LED_SCAN_MUX][LED_SCAN_LENGTH];

#if defined(__FOR_HUAIBEI__)
static unsigned char __displayBuffer[LED_DOT_HEIGHT + 16][LED_DOT_WIDTH / 8];
#else
static unsigned char __displayBuffer[LED_DOT_HEIGHT][LED_DOT_WIDTH / 8];
#endif


static int *__displayBufferBit;
static int *__scanBufferBit[LED_SCAN_MUX];

#define BIT_BAND_ADDR_SRAM(addr) ((int *)((int)(0x22000000 + (((int)(addr)) - 0x20000000)*32)))

const static unsigned char __dotArrayTable[] = {
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
	0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
	0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
	0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
	0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
	0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
	0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
	0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
	0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
	0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
	0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
	0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
	0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
	0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
	0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
	0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
	0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
	0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
	0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
	0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
	0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
	0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};


static unsigned char arrayBuffer[128];


const unsigned char *LedDisplayGB2312String32(int x, int y, int xend, int yend, const unsigned char *gbString) {
	int i, j;
	int xorg = x;
	if (!FontDotArrayFetchLock()) {
		return gbString;
	}

	if (xend > LED_DOT_WIDTH / 8) {
		xend = LED_DOT_WIDTH / 8;
	}
	if (yend > LED_DOT_HEIGHT) {
		yend = LED_DOT_HEIGHT;
	}

	while (*gbString) {
		if (isAsciiStart(*gbString)) {
			if (x > xend - BYTES_WIDTH_PER_FONT_ASCII_32X16) {
				y += BYTES_HEIGHT_PER_FONT_ASCII_32X16;
				x = xorg;
			}

			if (y > yend - BYTES_HEIGHT_PER_FONT_ASCII_32X16) {
				goto __exit;
			}

			j = FontDotArrayFetchASCII_32(arrayBuffer, *gbString++);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}


			for (i = 0; i < BYTES_HEIGHT_PER_FONT_ASCII_32X16; ++i) {
				for (j = 0; j < BYTES_WIDTH_PER_FONT_ASCII_32X16; j++) {
					__displayBuffer[y + i][j + x] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_ASCII_32X16 + j];
				}
			}
			x += BYTES_WIDTH_PER_FONT_ASCII_32X16;

		} else if (isGB2312Start(*gbString)) {
			int code;

			if (x > xend - BYTES_WIDTH_PER_FONT_GB_32X32) {
				y += BYTES_HEIGHT_PER_FONT_GB_32X32;
				x = xorg;
			}

			if (y > yend - BYTES_HEIGHT_PER_FONT_GB_32X32) {
				goto __exit;
			}

			code = (*gbString++) << 8;
			code += *gbString++;

			j = FontDotArrayFetchGB_32(arrayBuffer, code);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}

			for (i = 0; i < BYTES_HEIGHT_PER_FONT_GB_32X32; ++i) {
				for (j = 0; j < BYTES_WIDTH_PER_FONT_GB_32X32; j++) {
					__displayBuffer[y + i][j + x] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_GB_32X32 + j];
				}
			}
			x += BYTES_WIDTH_PER_FONT_GB_32X32;
		} else if (isUnicodeStart(*gbString)) {
			int code;

			if (x > LED_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_UCS_32X32) {
				y += BYTES_HEIGHT_PER_FONT_UCS_32X32;
				x = 0;
			}

			if (y > LED_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_UCS_32X32) {
				goto __exit;
			}

			code = (*gbString++) << 8;
			code += *gbString++;

			j = FontDotArrayFetchUCS_32(arrayBuffer, code);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}

			for (i = 0; i < BYTES_HEIGHT_PER_FONT_UCS_32X32; ++i) {
				for (j = 0; j < BYTES_WIDTH_PER_FONT_UCS_32X32; j++) {
					__displayBuffer[y + i][j + x] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_UCS_32X32 + j];
				}
			}			
			x += BYTES_WIDTH_PER_FONT_UCS_32X32;
		} else {
			++gbString;
		}
	}
__exit:
	FontDotArrayFetchUnlock();
	if (*gbString) {
		return gbString;
	}
	return NULL;
}

void LedDisplayGB2312String162(int x, int y, const unsigned char *gbString) {
	int i, j;
	if (!FontDotArrayFetchLock()) {
		return;
	}
	y += LED_DOT_HEIGHT;

	while (*gbString) {
		if (isAsciiStart(*gbString)) {
			if (x > LED_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_ASCII_16X8) {
				y += BYTES_HEIGHT_PER_FONT_ASCII_16X8;
				x = 0;
			}

			if (y > (LED_DOT_HEIGHT + 16) - BYTES_HEIGHT_PER_FONT_ASCII_16X8) {
				goto __exit;
			}

			j = FontDotArrayFetchASCII_16(arrayBuffer, *gbString++);
			for (i = 0; i < j; ++i) {
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i]];
			}

			for (i = 0; i < BYTES_HEIGHT_PER_FONT_ASCII_16X8; ++i) {
				__displayBuffer[y + i][x] = arrayBuffer[i];
			}
			x += BYTES_WIDTH_PER_FONT_ASCII_16X8;

		} else if (isGB2312Start(*gbString)) {
			int code = (*gbString++) << 8;
			if (!isGB2312Start(*gbString)) {
				goto __exit;
			}
			code += *gbString++;

			if (x > LED_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > (LED_DOT_HEIGHT + 16) - BYTES_HEIGHT_PER_FONT_GB_16X16) {
				goto __exit;
			}

			j = FontDotArrayFetchGB_16(arrayBuffer, code);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}

			for (i = 0; i < BYTES_HEIGHT_PER_FONT_GB_16X16; ++i) {
				for (j = 0; j < BYTES_WIDTH_PER_FONT_GB_16X16; j++) {
					__displayBuffer[y + i][j + x] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_GB_16X16 + j];
				}
			}
			x += BYTES_WIDTH_PER_FONT_GB_16X16;
		} else if (isUnicodeStart(*gbString)) {
			int code = (*gbString++) << 8;
			code += *gbString++;

			if (x > LED_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > LED_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_GB_16X16) {
				goto __exit;
			}

			j = FontDotArrayFetchUCS_16(arrayBuffer, code);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}

			for (i = 0; i < BYTES_HEIGHT_PER_FONT_GB_16X16; ++i) {
				for (j = 0; j < BYTES_WIDTH_PER_FONT_GB_16X16; j++) {
					__displayBuffer[y + i][j + x] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_GB_16X16 + j];
				}
			}
			x += BYTES_WIDTH_PER_FONT_GB_16X16;
		} else {
			++gbString;
		}
	}
__exit:
	FontDotArrayFetchUnlock();
}


bool LedDisplaySetPixel(int x, int y, int on) {
	if (x > LED_DOT_XEND) {
		return false;
	}
	if (y > LED_DOT_YEND) {
		return false;
	}
	__displayBufferBit[y * LED_DOT_WIDTH + x] = on ? 1 : 0;
	return true;
}

const unsigned char *LedDisplayGB2312String16(int x, int y, const unsigned char *gbString) {
	int i, j;
	if (!FontDotArrayFetchLock()) {
		return gbString;
	}

	while (*gbString) {
		if (isAsciiStart(*gbString)) {
			if (x > LED_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_ASCII_16X8) {
				y += BYTES_HEIGHT_PER_FONT_ASCII_16X8;
				x = 0;
			}

			if (y > LED_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_ASCII_16X8) {
				goto __exit;
			}

			j = FontDotArrayFetchASCII_16(arrayBuffer, *gbString);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}
			if (*gbString & 0x01) {
				for (i = 0; i < BYTES_HEIGHT_PER_FONT_ASCII_16X8; ++i) {
					__displayBuffer[y + i][x] = arrayBuffer[i + 1];
				}
			} else {
				for (i = 0; i < BYTES_HEIGHT_PER_FONT_ASCII_16X8; ++i) {
					__displayBuffer[y + i][x] = arrayBuffer[i];
				}
			}
			++gbString;
			x += BYTES_WIDTH_PER_FONT_ASCII_16X8;

		} else if (isGB2312Start(*gbString)) {
			int code = (*gbString++) << 8;
			if (!isGB2312Start(*gbString)) {
				goto __exit;
			}
			code += *gbString++;

			if (x > LED_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > LED_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_GB_16X16) {
				goto __exit;
			}

			j = FontDotArrayFetchGB_16(arrayBuffer, code);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}

			for (i = 0; i < BYTES_HEIGHT_PER_FONT_GB_16X16; ++i) {
				for (j = 0; j < BYTES_WIDTH_PER_FONT_GB_16X16; j++) {
					__displayBuffer[y + i][j + x] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_GB_16X16 + j];
				}
			}
			x += BYTES_WIDTH_PER_FONT_GB_16X16;
		} else if (isUnicodeStart(*gbString)) {
			int code = (*gbString++) << 8;
			code += *gbString++;

			if (x > LED_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > LED_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_GB_16X16) {
				goto __exit;
			}

			j = FontDotArrayFetchUCS_16(arrayBuffer, code);
			for (i = 0; i < j; i += 2) {
				unsigned char tmp = arrayBuffer[i];
				arrayBuffer[i] = __dotArrayTable[arrayBuffer[i + 1]];
				arrayBuffer[i + 1] = __dotArrayTable[tmp];
			}

			for (i = 0; i < BYTES_HEIGHT_PER_FONT_GB_16X16; ++i) {
				for (j = 0; j < BYTES_WIDTH_PER_FONT_GB_16X16; j++) {
					__displayBuffer[y + i][j + x] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_GB_16X16 + j];
				}
			}
			x += BYTES_WIDTH_PER_FONT_GB_16X16;
		} else {
			++gbString;
		}
	}
__exit:
	FontDotArrayFetchUnlock();
	if (*gbString) {
		return gbString;
	}
	return NULL;
}

void LedScanClear(int x, int y, int xend, int yend) {
	int vx;
	int *dest;

	for (; y <= yend; ++y) {
		dest = __scanBufferBit[y % LED_SCAN_MUX] + y / LED_SCAN_MUX + x * 8;
		for (vx = x; vx <= xend; ++vx) {
			*dest = 1;
			dest += 8;
		}
	}
}






#if defined(__FOR_HUAIBEI__)
void LedDisplayToScan2(int x, int y, int xend, int yend) {
	int vx;
	int *dest, *src;

	y += LED_DOT_HEIGHT;
	yend += LED_DOT_HEIGHT;

	for (; y <= yend; ++y) {
		src = __displayBufferBit + y * LED_SCAN_LENGTH + x;
		dest = __scanBufferBit[y  % LED_SCAN_MUX] + y / LED_SCAN_MUX + x * 8;
		for (vx = x; vx <= xend; ++vx) {
			*dest = !(*src++);
			dest += 8;
		}
	}
}
#endif



static inline void __ledScanHardwareInit() {
	GPIO_InitTypeDef GPIO_InitStructure;
	FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef FSMC_NORSRAMTimingInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	DMA_InitTypeDef DMA_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOG | RCC_APB2Periph_GPIOE |
						   RCC_APB2Periph_GPIOF, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 |
								  GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
								  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
								  GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* SRAM Address lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_12 | GPIO_Pin_13 |
								  GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								  GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NOE and NWE configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NE3 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	/* NE4 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	/* NBL0, NBL1 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	FSMC_NORSRAMTimingInitStructure.FSMC_AddressSetupTime = 0;  //FSMC_SetupTime
	FSMC_NORSRAMTimingInitStructure.FSMC_AddressHoldTime = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_DataSetupTime = 2;
	FSMC_NORSRAMTimingInitStructure.FSMC_BusTurnAroundDuration = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_CLKDivision = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_DataLatency = 0;
	FSMC_NORSRAMTimingInitStructure.FSMC_AccessMode = FSMC_AccessMode_A;


	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM4;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
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
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);

	GPIO_ResetBits(GPIOC, GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7);
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_7);
	GPIO_SetBits(GPIOC, GPIO_Pin_5);

	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel6_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM_TimeBaseStructure.TIM_Period = 92160 / LED_SCAN_MUX;
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_PrescalerConfig(TIM2, 8, TIM_PSCReloadMode_Immediate);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 8192;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
	TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);

	DMA_DeInit(DMA1_Channel6);
	DMA_InitStructure.DMA_PeripheralBaseAddr = 0;
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)0X6C001000;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = LED_SCAN_LENGTH;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
	DMA_InitStructure.DMA_MemoryInc     = DMA_MemoryInc_Disable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Low;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Enable;
	DMA_Init(DMA1_Channel6, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);
}


void LedScanOnOff(bool isOn) {
	if (isOn) {
		TIM_Cmd(TIM2, ENABLE);
	} else {
		TIM_Cmd(TIM2, DISABLE);
		GPIO_SetBits(GPIOC, GPIO_Pin_5);
	}
}

bool LedScanSetScanBuffer(int mux, int x, unsigned char dat) {
	if (x >= LED_SCAN_LENGTH) {
		return false;
	}
	if (mux >= LED_SCAN_MUX) {
		return false;
	}
	__scanBuffer[mux][x] = dat;
	return true;
}

void LedScanInit() {
	int i;

#if LED_DRIVER_LEVEL==1
	memset(__scanBuffer, 0x00, sizeof(__scanBuffer));
#else
	memset(__scanBuffer, 0xFF, sizeof(__scanBuffer));
#endif
	__displayBufferBit = BIT_BAND_ADDR_SRAM(__displayBuffer);

	for (i = 0; i < LED_SCAN_MUX; ++i) {
		__scanBufferBit[i] = BIT_BAND_ADDR_SRAM(__scanBuffer[i]);
	}

	__ledScanHardwareInit();
	FontDotArrayInit();
}

void TIM2_IRQHandler() {
	portBASE_TYPE tmp;
#define DMA1_Channel6_IT_Mask    ((uint32_t)(DMA_ISR_GIF6 | DMA_ISR_TCIF6 | DMA_ISR_HTIF6 | DMA_ISR_TEIF6))
	TIM2->SR = ~(0x00FF);
	DMA1_Channel6->CCR = 0x6042;
	DMA1->IFCR |= DMA1_Channel6_IT_Mask;
	DMA1_Channel6->CNDTR = LED_SCAN_LENGTH;
	DMA1_Channel6->CPAR = (uint32_t)(__scanBuffer[__scanLine]);
	tmp = GPIOC->ODR;
#if LED_OE_LEVEL==1
	tmp &= ~(1 << 5);
#elif LED_OE_LEVEL==0
	tmp |= (1 << 5);
#endif
	GPIOC->ODR = tmp;
	DMA1_Channel6->CCR = 0x6043;
}

void DMA1_Channel6_IRQHandler(void) {
	if (DMA_GetITStatus(DMA1_IT_TC6)) {
		portBASE_TYPE tmp;
		DMA_ClearITPendingBit(DMA1_IT_GL6);
		tmp = GPIOC->ODR;
#if LED_STROBE_PAUSE==1
		tmp &= ~(0x0F << 1);
		tmp |= __scanLine << 1 | (1 << 7);
#elif LED_STROBE_PAUSE==0
		tmp &= ~((0x0F << 1) | (1 << 7));
		tmp |= __scanLine << 1;
#endif
		GPIOC->ODR = tmp;
		if (++__scanLine >= LED_SCAN_MUX) {
			__scanLine = 0;
		}
#if LED_STROBE_PAUSE==1
		tmp &= ~(1 << 7);
#elif LED_STROBE_PAUSE==0
		tmp |= 1 << 7;
#endif
		GPIOC->ODR = tmp;
#if LED_OE_LEVEL==1
		tmp |= (1 << 5);
#elif LED_OE_LEVEL==0
		tmp &= ~(1 << 5);
#endif
		GPIOC->ODR = tmp;
	}
}

void LedDisplayClear(int x, int y, int xend, int yend) {
	x = x / 8;
	xend = xend / 8;
	for (; y <= yend; ++y) {
		int xv;
		for (xv = x; xv <= xend; ++xv) {
			__displayBuffer[y][xv] = 0;
		}

	}
}

#if defined(USE_QIANGLI_P10_1R1G) && USE_QIANGLI_P10_1R1G!=0
#include "led_qaingli_p10_1R1G.c"
#endif
#if defined(USE_NORMAL_16SCAN) && USE_NORMAL_16SCAN!=0
#include "led_normal_16scan.c"
#endif


