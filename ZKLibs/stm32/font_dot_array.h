#ifndef __FONT_DOT_ARRAY_H__
#define __FONT_DOT_ARRAY_H__

#include <stdint.h>
#include <norflash.h>
#include <FreeRTOS.h>

#define BYTES_HEIGHT_PER_FONT_GB_16X16 16
#define	BYTES_WIDTH_PER_FONT_GB_16X16 2
#define BYTES_HEIGHT_PER_FONT_ASCII_16X8 16
#define BYTES_WIDTH_PER_FONT_ASCII_16X8 1

#define BYTES_HEIGHT_PER_FONT_GB_32X32 32
#define	BYTES_WIDTH_PER_FONT_GB_32X32 4
#define BYTES_HEIGHT_PER_FONT_ASCII_32X16 32
#define BYTES_WIDTH_PER_FONT_ASCII_32X16 2

#define BYTES_HEIGHT_PER_FONT_GB_24X24 24
#define	BYTES_WIDTH_PER_FONT_GB_24X24 3
#define BYTES_HEIGHT_PER_FONT_ASCII_24X16 24
#define BYTES_WIDTH_PER_FONT_ASCII_24X16 2


void FontDotArrayInit(void);
static inline bool FontDotArrayFetchLock(void) {
	return NorFlashMutexLock(configTICK_RATE_HZ * 2);
}
static inline void FontDotArrayFetchUnlock(void) {
	NorFlashMutexUnlock();
}

int FontDotArrayFetchASCII_16(uint8_t *buf, uint8_t c);
int FontDotArrayFetchASCII_32(uint8_t *buf, uint8_t c);
int FontDotArrayFetchGB_16(uint8_t *buf, uint16_t code);
int FontDotArrayFetchGB_32(uint8_t *buf, uint16_t code);


static inline int isGB2312Start(uint8_t c) {
	return (c >= 0xA1);
}

static inline int isAsciiStart(uint8_t c) {
	return (c <= 0x7F);
}

static inline int isGB2312(uint16_t code) {
	return (code >= 0xA1A1) && (code <= 0xF7FE);
}

static inline int isAscii(uint16_t code) {
	return (code >= 0x20) && (code <= 0x7F);
}

static inline int isUnicode(uint16_t code) {
	return (code >= 0x9000) && (code <= 0xA1A1);
}


#endif
