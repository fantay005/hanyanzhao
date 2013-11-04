#ifndef __PREINCLUDE_H__
#define __PREINCLUDE_H__

#include "preinclude_commoh.h"

//利辛县, 强力双色LED, 32*9
#define __LED__                          1
#define __LED_HUAIBEI__                  1
#define __TARGET_STRING__                "LED_HUAIBEI"


#define USE_NORMAL_16SCAN                1


/// LED扫描模式, 可以是16,8,4,2; 分别是1/16,1/8,1/4,1/2扫描;
#define LED_SCAN_MUX 16

/// 扫描链的长度
#define LED_SCAN_LENGTH 128
#define LED_SCAN_BITS_MASK 0x0F
#define LED_SCAN_BITS 4

#define LED_DOT_HEIGHT 32
#define LED_DOT_WIDTH LED_SCAN_LENGTH
#define LED_DRIVER_LEVEL 0

#else
#  error "Preinclude file can only be included once in command line with --preinclude=xxxx"
#endif
