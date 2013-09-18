#ifndef __LEDCONFIG_H__
#define __LEDCONFIG_H__



/// LED扫描模式, 可以是16,8,4,2; 分别是1/16,1/8,1/4,1/2扫描;
#define LED_SCAN_MUX 16

/// 扫描链的长度
#define LED_SCAN_LENGTH 192
#define LED_SCAN_BITS_MASK 0x0F
#define LED_SCAN_BITS 4

#define LED_DOT_HEIGHT 48
#define LED_DOT_WIDTH LED_SCAN_LENGTH

#define LED_DOT_XEND (LED_DOT_WIDTH-1)
#define LED_DOT_YEND (LED_DOT_HEIGHT-1)

#endif // __LEDCONFIG_H__
