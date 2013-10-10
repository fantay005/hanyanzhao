/*
 * =====================================================================================
 *
 *       Filename:  sms.h
 *
 *    Description:  sms decode and encode headers
 *
 *        Version:  1.0
 *        Created:  2008-6-24 9:10:50
 *       Revision:  none
 *       Compiler:  C51
 *
 *         Author:  xiqingping(xiqingping@gmail.com)
 *        Company:  Beijin Cabletech development LTD.
 *
 * =====================================================================================
 */
#ifndef __SMS_H__
#define __SMS_H__

#define PDU_NUMBER_TYPE_INTERNATIONAL	0x91
#define PDU_NUMBER_TYPE_NATIONAL		0x81
#define GPRS_NUMBER_TYPE				0xFF

#define ENCODE_TYPE_GBK  0
#define ENCODE_TYPE_UCS2 1

typedef struct {
	unsigned char numberType;
	unsigned char encodeType;
	unsigned char contentLen;
	char number[15];
	char time[15];
	char content[162];
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


