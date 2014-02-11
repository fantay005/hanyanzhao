#ifdef __LED__
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
#include "led_lowlevel.h"


void LedDisplayClearAll() {
	memset(__displayBuffer, 0, sizeof(__displayBuffer));
}


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

	x = x / 8;

	if (xend > LED_VIR_DOT_WIDTH / 8) {
		xend = LED_VIR_DOT_WIDTH / 8;
	}
	if (yend > LED_VIR_DOT_HEIGHT) {
		yend = LED_VIR_DOT_HEIGHT;
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

			if (x > LED_VIR_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_UCS_32X32) {
				y += BYTES_HEIGHT_PER_FONT_UCS_32X32;
				x = 0;
			}

			if (y > LED_VIR_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_UCS_32X32) {
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
	return gbString;
}


#if 1
const unsigned char *LedDisplayGB2312String32Scroll(int x, int y, int dx, const unsigned char *gbString) {
	int i, j, dxin;
	if (!FontDotArrayFetchLock()) {
		return gbString;
	}
	dxin = dx;

	x = x / 8;
	dxin = dxin / 8;

	while (*gbString) {
		if (x >= LED_VIR_DOT_WIDTH / 8) {
			x -= LED_VIR_DOT_WIDTH / 8;
		}
		if (isAsciiStart(*gbString)) {
			if (dxin < BYTES_WIDTH_PER_FONT_ASCII_32X16) {
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
					if (j + x >= LED_VIR_DOT_WIDTH / 8) {
						__displayBuffer[y + i][j + x - LED_VIR_DOT_WIDTH / 8] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_ASCII_32X16 + j];
					} else if (j + x < LED_VIR_DOT_WIDTH / 2 / 8) {
						__displayBuffer[y + i][j + x + LED_VIR_DOT_WIDTH / 8] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_ASCII_32X16 + j];
					}
				}
			}

			x += BYTES_WIDTH_PER_FONT_ASCII_32X16;
			dxin -= BYTES_WIDTH_PER_FONT_ASCII_32X16;

		} else if (isGB2312Start(*gbString)) {
			int code;
			if (dxin < BYTES_WIDTH_PER_FONT_GB_32X32) {
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
					if (j + x >= LED_VIR_DOT_WIDTH / 8) {
						__displayBuffer[y + i][j + x - LED_VIR_DOT_WIDTH / 8] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_GB_32X32 + j];
					} else if (j + x < LED_VIR_DOT_WIDTH / 2 / 8) {
						__displayBuffer[y + i][j + x + LED_VIR_DOT_WIDTH / 8] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_GB_32X32 + j];
					}
				}
			}
			x += BYTES_WIDTH_PER_FONT_GB_32X32;
			dxin -= BYTES_WIDTH_PER_FONT_GB_32X32;
		} else if (isUnicodeStart(*gbString)) {
			int code;

			if (dxin < BYTES_WIDTH_PER_FONT_UCS_32X32) {
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
					if (j + x >= LED_VIR_DOT_WIDTH / 8) {
						__displayBuffer[y + i][j + x - LED_VIR_DOT_WIDTH / 8] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_UCS_32X32 + j];
					} else if (j + x < LED_VIR_DOT_WIDTH / 2 / 8) {
						__displayBuffer[y + i][j + x + LED_VIR_DOT_WIDTH / 8] = arrayBuffer[i * BYTES_WIDTH_PER_FONT_UCS_32X32 + j];
					}
				}
			}
			x += BYTES_WIDTH_PER_FONT_UCS_32X32;
			dxin -= BYTES_WIDTH_PER_FONT_UCS_32X32;
		} else {
			++gbString;
		}
	}
__exit:
	FontDotArrayFetchUnlock();
	return gbString;
}

#endif


void LedDisplayGB2312String162(int x, int y, const unsigned char *gbString) {
	int i, j;
	if (!FontDotArrayFetchLock()) {
		return;
	}
	y += LED_VIR_DOT_HEIGHT;

	while (*gbString) {
		if (isAsciiStart(*gbString)) {
			if (x > LED_VIR_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_ASCII_16X8) {
				y += BYTES_HEIGHT_PER_FONT_ASCII_16X8;
				x = 0;
			}

			if (y > (LED_VIR_DOT_HEIGHT + 16) - BYTES_HEIGHT_PER_FONT_ASCII_16X8) {
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

			if (x > LED_VIR_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > (LED_VIR_DOT_HEIGHT + 16) - BYTES_HEIGHT_PER_FONT_GB_16X16) {
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

			if (x > LED_VIR_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > LED_VIR_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_GB_16X16) {
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
	if (x > LED_VIR_DOT_WIDTH - 1) {
		return false;
	}
	if (y > LED_VIR_DOT_HEIGHT - 1) {
		return false;
	}
	*((unsigned int *)(__displayBufferBit + (y * LED_VIR_DOT_WIDTH + x) * 4)) = on ? 1 : 0;
	return true;
}

const unsigned char *LedDisplayGB2312String16(int x, int y, const unsigned char *gbString) {
	int i, j;
	if (!FontDotArrayFetchLock()) {
		return gbString;
	}

	while (*gbString) {
		if (isAsciiStart(*gbString)) {
			if (x > LED_VIR_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_ASCII_16X8) {
				y += BYTES_HEIGHT_PER_FONT_ASCII_16X8;
				x = 0;
			}

			if (y > LED_VIR_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_ASCII_16X8) {
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

			if (x > LED_VIR_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > LED_VIR_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_GB_16X16) {
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

			if (x > LED_VIR_DOT_WIDTH / 8 - BYTES_WIDTH_PER_FONT_GB_16X16) {
				y += BYTES_HEIGHT_PER_FONT_GB_16X16;
				x = 0;
			}

			if (y > LED_VIR_DOT_HEIGHT - BYTES_HEIGHT_PER_FONT_GB_16X16) {
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
	return gbString;
	if (*gbString) {
		return gbString;
	}
	return NULL;
}



#if defined(__LED_HUAIBEI__) && (__LED_HUAIBEI__!=0)
void LedDisplayToScan2(int x, int y, int xend, int yend) {
	int vx;
	int *dest, *src;

	y += LED_VIR_DOT_HEIGHT;
	yend += LED_VIR_DOT_HEIGHT;

	for (; y <= yend; ++y) {
		src =(int *)( __displayBufferBit + y * LED_SCAN_LENGTH + x);
		dest = (int *)(__scanBufferBit + y / LED_SCAN_MUX + x * 8);
		for (vx = x; vx <= xend; ++vx) {
			*dest = !(*src++);
			dest += 8;
		}
	}
}
#endif


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


void LedDisplayInit() {
	LedDisplayClearAll();
	__displayBufferBit = BIT_BAND_ADDR_SRAM(__displayBuffer);
	FontDotArrayInit();
}



#endif
