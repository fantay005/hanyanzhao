#ifndef __LEDCONFIG_H__
#define __LEDCONFIG_H__

/// LED扫描模式, 可以是16,8,4,2; 分别是1/16,1/8,1/4,1/2扫描;
/// #define LED_SCAN_MUX 4

/// 扫描链的长度
/// #define LED_SCAN_LENGTH (128)

/// #define LED_DOT_HEIGHT 16
/// #define LED_DOT_WIDTH (32)


/// #define LED_DRIVER_LEVEL  1    // 驱动电平，0或1
/// #define LED_STROBE_PAUSE  0    // LED_LT脉冲电平，0或1
/// #define LED_OE_LEVEL 0

// #define USE_QIANGLI_P10_1R1G 1
// #define QIANGLI_UNIT_X_NUM 12
// #define QIANGLI_UNIT_Y_NUM 2

#if defined(USE_QIANGLI_P10_1R1G) && (USE_QIANGLI_P10_1R1G!=0)
#  define LED_SCAN_MUX 4
#  define LED_SCAN_LENGTH (32*4*QIANGLI_UNIT_X_NUM)
#  define LED_DOT_WIDTH (32*QIANGLI_UNIT_X_NUM)
#  define LED_DOT_HEIGHT (32*QIANGLI_UNIT_Y_NUM)
#  define LED_DRIVER_LEVEL  1    // 驱动电平，0或1
#  define LED_STROBE_PAUSE  1    // LED_LT脉冲电平，0或1
#  define LED_OE_LEVEL 0
#endif

#define LED_DOT_XEND (LED_DOT_WIDTH-1)
#define LED_DOT_YEND (LED_DOT_HEIGHT-1)

#endif // __LEDCONFIG_H__
