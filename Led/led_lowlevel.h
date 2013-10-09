#ifndef __LED_LOWLEVEL_H__
#define __LED_LOWLEVEL_H__

#include <stdbool.h>

#include "ledconfig.h"

bool LedScanSetScanBuffer(int mux, int x, unsigned char dat);
bool LedDisplaySetPixel(int x, int y, int on);
void LedScanInit(void);
void LedScanOnOff(bool isOn);
void LedScanClear(int x, int y, int xend, int yend);
void LedDisplayToScan(int x, int y, int xend, int yend);
#if defined(__LED_HUAIBEI__) && (__LED_HUAIBEI__!=0)
void LedDisplayToScan2(int x, int y, int xend, int yend);
void LedDisplayGB2312String162(int x, int y, const unsigned char *gbString);
#endif
const unsigned char *LedDisplayGB2312String16(int x, int y, const unsigned char *gbString);
void LedDisplayGB2312String162(int x, int y, const unsigned char *gbString);
//const unsigned char *LedDisplayGB2312String32(int x, int y, const unsigned char *gbString);
const unsigned char *LedDisplayGB2312String32(int x, int y, int xend, int yend, const unsigned char *gbString);
void LedDisplayClear(int x, int y, int xend, int yend);

#endif
