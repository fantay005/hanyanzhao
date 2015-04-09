#ifndef __PREINCLUDE_H__
#define __PREINCLUDE_H__

#include "preinclude_commoh.h"

//GPRS¿Æ∞»≈‰÷√
#define __SPEAKER__                     1
#define __SPEAKER_V3__                  1

#define __TARGET_STRING__               "GATEWAY"

#else
#  error "Preinclude file can only be included once in command line with --preinclude=xxxx"
#endif
