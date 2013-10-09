#ifndef __UNICODE2GBK_H__
#define __UNICODE2GBK_H__

#include <stdint.h>
#include "FreeRTOS.h"

/// \brief  把一串UCS2编码的字符转换成GBK编码的字符.
/// \param  unicode   需要转换的UCS2编码字符串.
/// \param  len       需要转换的UCS2编码字符串的字节数(非字符数).
/// \return !=NULL    转换好的GBK字符串,以0x00结束,使用之后必须用Unicode2GBKDestroy释放内存.
/// \return ==NULL    转换时申请内存失败.
uint8_t *Unicode2GBK(const uint8_t *unicode, int len);

/// \brief  释放由UCS2转换好的GBK字符串.
/// \param  p   需要释放的GBK字符串.
static inline void Unicode2GBKDestroy(uint8_t *p) {
	vPortFree(p);
}

#endif
