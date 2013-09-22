#ifndef __LED_LOWLEVEL_H__
#define __LED_LOWLEVEL_H__

#include <stdbool.h>

void LedScanInit(void);
void LedScanOnOff(bool isOn);
void LedScanClear(int x, int y, int xend, int yend);
void LedDisplayToScan(int x, int y, int xend, int yend);
void LedDisplayGB2312String16(int x, int y, const unsigned char *gbString);
void LedDisplayGB2312String32(int x, int y, const unsigned char *gbString);

#endif
