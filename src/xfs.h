#ifndef __XFS__H__
#define	__XFS__H__

typedef enum {
	TYPE_GB2312 = 0x00,
	TYPE_GBK = 0x01,
	TYPE_BIG5 = 0x02,
	TYPE_UCS2 = 0x03,
} XfsEncodeType;

void xfsSpeak(const char *s, int len, XfsEncodeType type);

#endif
