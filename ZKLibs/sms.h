#ifndef __SMS_H__
#define __SMS_H__

#include <stdint.h>

#define PDU_NUMBER_TYPE_INTERNATIONAL	0x91
#define PDU_NUMBER_TYPE_NATIONAL		0x81
#define GPRS_NUMBER_TYPE				0xFF

#define ENCODE_TYPE_GBK  0
#define ENCODE_TYPE_UCS2 1

typedef struct {
	uint8_t numberType;
	uint8_t encodeType;
	uint16_t contentLen;
	uint8_t number[15];
	int8_t time[15];
	int8_t content[700];
} SMSInfo;

/// \brief  解码PDU短信.
/// \param  pdu[in]     需要解码的PDU.
/// \param  sms[out]    用于存放解码之后的短信信息.
void SMSDecodePdu(const char *pdu, SMSInfo *sms);

/// \brief  用8bit方式把数据编码成PDU.
/// \param  pdu[out]    用于存放编码之后的PDU数据.
/// \param  destNum[in] 短信发送的目标号码.
/// \param  dat[in]     需要编码的字符串数据.
/// \return 编码后PDU串的字节长度.
/// \note   只能编码ASCII字符串.
int SMSEncodePdu8bit(char *pdu, const char *destNum, const char *dat);

/// \brief  用UCS2方式把数据编码成PDU.
/// \param  pdu[out]    用于存放编码之后的PDU数据.
/// \param  destNum[in] 短信发送的目标号码.
/// \param  dat[in]     需要编码的字符串数据.
/// \param  len[in]     需要编码的字符串数据的字节长度.
/// \return 编码后PDU串的字节长度.
/// \note   只能编码UCS2数据.
int SMSEncodePduUCS2(char *pdu, const char *destNum, const char *ucs2, int len);

#endif // ifndef __SMS_H__


