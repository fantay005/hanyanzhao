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
#include <string.h>
#include "sms.h"


#define GSM_ENCODE_7BIT		0
#define GSM_ENCODE_8BIT		4
#define GSM_ENCODE_UCS2		8


static inline int string2bytes(unsigned char *pd, const char *ps, int len) {
	unsigned char ch0, ch1;
	int i = len;
	for (; i > 0;) {
		ch0 = *ps++;
		i--;
		if ((ch0 >= '0') && (ch0 <= '9')) {
			ch0 = (ch0 - '0') << 4;
		} else {
			ch0 = (ch0 - ('A' - 10)) << 4;
		}

		ch1 = *ps++;
		i--;
		if ((ch1 >= '0') && (ch1 <= '9')) {
			ch1 = ch1 - '0';
		} else {
			ch1 = ch1 - ('A' - 10);
		}

		*pd++ = (ch0 | ch1);
	}
	return (len + 1) / 2;
}

static inline unsigned char sms_serializeNumbers(char *pd, const char *ps, unsigned char len) {
	unsigned char i;
	char ch;

	for (i = 0; i < len; i += 2) {
		ch = *ps++;
		*pd++ = *ps++;
		*pd++ = ch;
	}
	if (*(pd - 1) == 'F') {
		pd--;
		len--;
	}
	*pd = 0;
	return len;
}

static inline unsigned char sms_decode7bit(char *pd, const char *pdu_ud, unsigned char len) {
	unsigned char ch, ch_p;
	unsigned char cnt;
	unsigned char rc;
	unsigned char temp;

	cnt = 0;
	ch_p = 0;

	for (rc = 0; rc < len;) {
		temp = *pdu_ud++;
		if ((temp >= '0') && (temp <= '9')) {
			ch = (temp - '0') << 4;
		} else {
			ch = (temp - ('A' - 10)) << 4;
		}
		temp = *pdu_ud++;
		if ((temp >= '0') && (temp <= '9')) {
			ch |= (temp - '0');
		} else {
			ch |= (temp - ('A' - 10));
		}

		*pd++ = ((ch << cnt) | ch_p) & 0x7F;
		ch_p = ch >> (7 - cnt);
		rc++;
		cnt++;
		if (cnt == 7) {
			*pd++ = ch_p;
			rc++;
			cnt = 0;
			ch_p = 0;
		}
	}
	*pd = 0;
	return rc;
}

static inline unsigned char sms_decode8bit(char *pd, const char *pdu_ud, unsigned char len) {
	len = string2bytes((unsigned char *)pd, pdu_ud, len * 2);
	pd[len] = 0;
	return len;
}

static inline unsigned char sms_decodeucs2(char *pd, const char *pdu_ud, unsigned char len) {
	unsigned char ch0, ch1, temp;
	unsigned char i, rc = 0;

	for (i = 0; i < len;) {
		temp = *pdu_ud++;
		if ((temp >= '0') && (temp <= '9')) {
			ch0 = (temp - '0') << 4;
		} else {
			ch0 = (temp - ('A' - 10)) << 4;
		}
		temp = *pdu_ud++;
		if ((temp >= '0') && (temp <= '9')) {
			ch0 |= (temp - '0');
		} else {
			ch0 |= (temp - ('A' - 10));
		}
		i++;

		temp = *pdu_ud++;
		if ((temp >= '0') && (temp <= '9')) {
			ch1 = (temp - '0') << 4;
		} else {
			ch1 = (temp - ('A' - 10)) << 4;
		}
		temp = *pdu_ud++;
		if ((temp >= '0') && (temp <= '9')) {
			ch1 |= (temp - '0');
		} else {
			ch1 |= (temp - ('A' - 10));
		}
		i++;
		*pd++ = ch1;
		*pd++ = ch0;
		rc += 2;
	}
	*pd = 0;
	return rc;
}

//07917238010010F5040BC87238880900F100009930925161958003C16010
//07 917238010010F5 04 0B C8 7238880900F1 0000 99309251619580 03C16010
void Sms_DecodePdu(const char *pdu, sms_t *psms) {
	unsigned char temp;
	unsigned char dcs;

	string2bytes(&temp, pdu, 2);	//获取SMSC长度
	pdu += temp * 2 + 4;				// pdu = "0BC8....."

	string2bytes(&temp, pdu, 2);	//获取发送端号码
	if (temp & 1) {
		temp += 1;
	}
	pdu += 2;						// pdu = "C8723888..."

	string2bytes(&(psms->number_type), pdu, 2);
	pdu += 2;						// pdu = "723888..."

	sms_serializeNumbers(psms->number, pdu, temp);
	pdu += temp;					// pdu = "0000993..."
	pdu += 2;						// pdu = "009930...."

	string2bytes(&dcs, pdu, 2);	//获取编码方式
	pdu += 2;						// pdu = "993092...."
	sms_serializeNumbers(psms->time, pdu, 14);	//获取时间
	pdu += 14;						//pdu = "03C16010"
	string2bytes(&temp, pdu, 2);	//信息长度
	pdu += 2;						//pdu = "C16010"

	if (dcs == GSM_ENCODE_7BIT) {
		psms->encode_type = ENCODE_TYPE_GBK;
		psms->content_len = sms_decode7bit(psms->sms_content, pdu, temp);
	} else if (dcs == GSM_ENCODE_8BIT) {
		psms->encode_type = ENCODE_TYPE_GBK;
		psms->content_len = sms_decode8bit(psms->sms_content, pdu, temp);
	} else if (dcs == GSM_ENCODE_UCS2) {
		psms->encode_type = ENCODE_TYPE_UCS2;
		psms->content_len = sms_decodeucs2(psms->sms_content, pdu, temp);
	}
}

/// 以8bit格式编码PDU.
//  \param out 用于存放编码后的数据;
//  \param dest_num 发送的目标号码.
//  \param dat 要编码的数据, 以'\0'结束;
//  \return >0 编码后数据的长度;
//  \return 0 失败;
//  \note 返回的数据长度是PDU中从目标号码开始的长度, 最前面还有一个字节的SMSC长度, 所以串口发送时要发送的数据比PDU的数据多一个字节;
//  \note 保存到out中的数据是2进制数据, 发送时必须吧每个字节的数据拆分成2个16进制字符发送;
unsigned char SMS_EncodePdu8bit(char *out, char *dest_num, char *dat) {
	unsigned char rc;
	unsigned char ch;
	if (0 == out) {
		return 0;
	}
	if (0 == dest_num) {
		return 0;
	}
	if (0 == dat) {
		return 0;
	}
	if (((*dest_num < '0') || (*dest_num > '9')) &&
			(*dest_num != '+')) {
		return 0;
	}

	*out++ = 0x00;
	*out++ = 0x31;
	*out++ = 0x00;
	if (*dest_num == '+') {
		dest_num++;
		*out++ = strlen(dest_num);
		*out++ = PDU_NUMBER_TYPE_INTERNATIONAL;
	} else {
		*out++ = strlen(dest_num);
		*out++ = PDU_NUMBER_TYPE_NATIONAL;
	}
	rc = 6;
	while (1) {
		if (0 == *dest_num) {
			break;
		}
		ch  = *dest_num++ & 0x0F;
		if (0 == *dest_num) {
			*out++ = 0xF0 | ch;
			break;
		}
		*out++ = ((*dest_num) << 4) | ch;
		rc++;
	}
	*out++ = 0x00;
	*out++ = GSM_ENCODE_8BIT;
	*out++ = 0xA7;
	*out++ = strlen(dat);
	rc += 4;
	while (*dat) {
		*out++ = *dat++;
		rc++;
		if (rc > 155) {
			return 0;
		}
	}
	return rc;
}

#if 0
/// 以8bit格式编码PDU.
//  \param out 用于存放编码后的数据;
//  \param dest_num 发送的目标号码.
//  \param dat 要编码的数据, 以'\0'结束;
//  \return >0 编码后数据的长度.
//  \return 0 失败.
unsigned char Sms_EncodePdu_8bit(char *pdst, sms_t *psms, unsigned char *pdulen) reentrant {
	unsigned char ch0, ch1, i;
	unsigned char rc = 0;

	*pdst++ = 0x00;
	rc++;		// SMSC, 使用卡内设置的SMSC
	*pdst++ = 0x31;
	rc++;		// PDUType
	*pdst++ = 0x00;
	rc++;		// MR
	*pdst++ = strlen(psms->number);
	rc++;		// DA 长度
	*pdst++ = psms->number_type;
	rc++;			// DA type

	i = 0;										// DA
	while (1) {
		ch0 = psms->number[i++];
		ch1 = psms->number[i++];
		if (ch0 == 0) {
			break;
		}
		if (ch1 == 0) {
			*pdst++ = 0xF0 | ch0;
			rc++;
			break;
		}
		*pdst++ = (ch1 << 4) | (ch0 & 0x0F);
		rc++;
	}

	*pdst++ = 0x00;
	rc++;				// PID
	*pdst++ = GSM_ENCODE_8BIT;
	rc++; 	// DSC
	*pdst++ = 0xA7;
	rc++;				// VP
	*pdst++ = strlen(psms->sms_content);
	rc++;					// UDL
	i = 0;
	while (1) {
		ch0 = psms->sms_content[i++];
		if (ch0 == 0) {
			break;
		}
		*pdst++ = ch0;
		rc++;
	}
	*pdulen = rc - 1;
	return rc;
}
unsigned char Sms_EncodePdu_7bit(char *pdst, sms_t *psms, unsigned char *pdulen) reentrant {
	unsigned char ch0, ch1, i, cnt;
	unsigned char rc = 0;

	*pdst++ = 0x00;
	rc++;		// SMSC, 使用卡内设置的SMSC
	*pdst++ = 0x31;
	rc++;		// PDUType
	*pdst++ = 0x00;
	rc++;		// MR
	*pdst++ = strlen(psms->number);
	rc++;		// DA 长度
	*pdst++ = psms->number_type;
	rc++;			// DA type

	i = 0;										// DA
	while (1) {
		ch0 = psms->number[i++];
		ch1 = psms->number[i++];
		if (ch0 == 0) {
			break;
		}
		if (ch1 == 0) {
			*pdst++ = 0xF0 | ch0;
			rc++;
			break;
		}
		*pdst++ = (ch1 << 4) | (ch0 & 0x0F);
		rc++;
	}

	*pdst++ = 0x00;
	rc++;				// PID
	*pdst++ = GSM_ENCODE_7BIT;
	rc++; 	// DSC
	*pdst++ = 0xA7;
	rc++;				// VP
	*pdst++ = strlen(psms->sms_content);
	rc++;				// PID

	i = 0;
	cnt = 0;
	ch0 = 0;
	while (1) {
		if (cnt == 0) {
			ch0 = psms->sms_content[i++];
			if (ch0 == 0) {
				break;
			}
		} else {
			ch0 = ch1 >> cnt;
		}
		ch1 = psms->sms_content[i++];
		if (ch1 == 0) {
			*pdst = ch0;
			rc++;
			break;
		}
		*pdst++ = (ch1 << (7 - cnt)) | ch0;
		rc++;
		cnt++;
		if (cnt >= 7) {
			cnt = 0;
		}
	}
	*pdulen = rc - 1;
	return rc;

}
#endif
