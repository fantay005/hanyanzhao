#ifndef __SOFT_PWM_LED_H__
#define __SOFT_PWM_LED_H__

#include <stdbool.h>

enum SoftPWNLedColor {
	SoftPWNLedColorRed,
	SoftPWNLedColorOrange,
	SoftPWNLedColorBlue,
	SoftPWNLedColorYellow,
	SoftPWNLedColorNULL,
};

bool SoftPWNLedSetColor(enum SoftPWNLedColor color);

#endif
