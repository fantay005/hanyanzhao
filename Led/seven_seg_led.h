#ifndef __SEVEN_SEG_LEG_HH__
#define __SEVEN_SEG_LEG_HH__

#include <stdbool.h>

void SevenSegLedInit(void);
bool SevenSegLedSetContent(unsigned int index, char what);
void SevenSegLedDisplay(void);

#endif
