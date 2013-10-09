#ifndef __SOFT_PWM_LED_H__
#define __SOFT_PWM_LED_H__

enum SoftPWNLedColor {
	SoftPWNLedColorRed,
	SoftPWNLedColorOrange,
	SoftPWNLedColorBlue,
	SoftPWNLedColorYellow,
	SoftPWNLedColorNULL,
};

void SoftPWNLedSetColor(enum SoftPWNLedColor color);

#endif