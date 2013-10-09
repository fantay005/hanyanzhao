#ifndef __PREINCLUDE_H__
#define __PREINCLUDE_H__

#include "preinclude_commoh.h"

//利辛县, 强力双色LED, 32*12
#define __LED__                          1
#define __LED_LIXIN_384__                1

#define USE_QIANGLI_P10_1R1G             1
#define QIANGLI_UNIT_X_NUM               12
#define QIANGLI_UNIT_Y_NUM               2

#else
#  error "Preinclude file can only be included once in command line with --preinclude=xxxx"
#endif
