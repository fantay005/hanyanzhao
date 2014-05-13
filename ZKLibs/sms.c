#include <string.h>
#include "sms.h"

#define GSM_SMS_ENCODE_7BIT		0
#define GSM_SMS_ENCODE_8BIT		4
#define GSM_SMS_ENCODE_UCS2		8

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

static inline int sms_decode7bit(char *pd, const char *pdu_ud, int len) {
	unsigned char ch, ch_p;
	int rc, cnt;
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

static inline int sms_decode8bit(char *pd, const char *pdu_ud, int len) {
	len = string2bytes((unsigned char *)pd, pdu_ud, len * 2);
	pd[len] = 0;
	return len;
}

static inline int sms_decodeucs2(char *pd, const char *pdu_ud, int len) {
	unsigned char ch0, ch1, temp;
	int i, rc = 0;

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
//07 91 7238010010F5 04 0B C8 7238880900F1 0000 99309251619580 03C16010
static unsigned char n;
static char buffer[1340];

void SMSDecodePdu(const char *pdu, SMSInfo *psms) {
	unsigned char temp, dcs, F0, Total, Sequence, i, LONGSMS = 0;

	string2bytes(&temp, pdu, 2);	//获取SMSC长度
	pdu += temp * 2 + 2;				// pdu = "0BC8....."

	string2bytes(&F0, pdu, 2);   //获取FO信息第一字节
	if(F0 & 0x40) {
		LONGSMS = 1;
	}
	pdu += 2;
	
	string2bytes(&temp, pdu, 2);	//获取发送端号码长度
	if (temp & 1) {
		temp += 1;
	}
	pdu += 2;						// pdu = "C8723888..."

	string2bytes(&(psms->numberType), pdu, 2);
	pdu += 2;						// pdu = "723888..."

	sms_serializeNumbers((char *)psms->number, pdu, temp);
	pdu += temp;					// pdu = "0000993..."
	pdu += 2;						// pdu = "009930...."

	string2bytes(&dcs, pdu, 2);	//获取编码方式
	pdu += 2;						// pdu = "993092...."
	sms_serializeNumbers((char *)psms->time, pdu, 14);	//获取时间
	pdu += 14;						//pdu = "03C16010"
	string2bytes(&temp, pdu, 2);	//信息长度
	pdu += 2;						//pdu = "C16010"
	
	if(LONGSMS == 1){
		pdu += 8;
		
		string2bytes(&Total, pdu, 2);  //获取长短信包含几条短信
		pdu += 2;
		
		string2bytes(&Sequence, pdu, 2);  //获取长短信 各短信顺序
		pdu += 2;
		
		if(Sequence == 1){
			memcpy(&buffer, pdu, 134 * 2);
		}
		
		for(i = 0; i < Total; i++){
			if(Sequence == (i + 1)){
				memcpy(&buffer[134 * i * 2], pdu, (temp - 6) * 2);
			}
		}
    n++;
		if(n != Total){
			psms->encodeType = ENCODE_TYPE_UCS2;
			psms->contentLen = 0;
		} else {
				if (dcs == GSM_SMS_ENCODE_7BIT) {
					psms->encodeType = ENCODE_TYPE_GBK;
					psms->contentLen = sms_decode7bit((char *)psms->content, (const char *)&buffer, (134 * (Total - 1) + temp - 6));
				} else if (dcs == GSM_SMS_ENCODE_8BIT) {
					psms->encodeType = ENCODE_TYPE_GBK;
					psms->contentLen = sms_decode8bit((char *)psms->content, (const char *)&buffer, (134 * (Total - 1) + temp - 6));
				} else if (dcs == GSM_SMS_ENCODE_UCS2) {
					psms->encodeType = ENCODE_TYPE_UCS2;
					psms->contentLen = sms_decodeucs2((char *)psms->content, (const char *)&buffer, (134 * (Total - 1) + temp - 6));
				}
				n = 0;
    }
	} else {
			if (dcs == GSM_SMS_ENCODE_7BIT) {
				psms->encodeType = ENCODE_TYPE_GBK;
				psms->contentLen = sms_decode7bit((char *)psms->content, pdu, temp);
			} else if (dcs == GSM_SMS_ENCODE_8BIT) {
				psms->encodeType = ENCODE_TYPE_GBK;
				psms->contentLen = sms_decode8bit((char *)psms->content, pdu, temp);
			} else if (dcs == GSM_SMS_ENCODE_UCS2) {
				psms->encodeType = ENCODE_TYPE_UCS2;
				psms->contentLen = sms_decodeucs2((char *)psms->content, pdu, temp);
			}
	}
}

int SMSEncodePdu8bit(char *pdu, const char *destNum, const char *dat) {
	int rc;
	unsigned char ch;
	if (0 == pdu) {
		return 0;
	}
	if (0 == destNum) {
		return 0;
	}
	if (0 == dat) {
		return 0;
	}
	if ((*destNum < '0') || (*destNum > '9')) {
		return 0;
	}

	*pdu++ = 0x00;
	*pdu++ = 0x11;
	*pdu++ = 0x00;
	*pdu++ = strlen(destNum);
	if (strncmp(destNum, "86", 2) == 0) {
		*pdu++ = PDU_NUMBER_TYPE_INTERNATIONAL;
	} else {
		*pdu++ = PDU_NUMBER_TYPE_NATIONAL;
	}

	rc = 6;
	while (1) {
		if (0 == *destNum) {
			break;
		}
		ch  = *destNum++ & 0x0F;
		if (0 == *destNum) {
			*pdu++ = 0xF0 | ch;
			break;
		}
		*pdu++ = ((*destNum++) << 4) | ch;
		rc++;
	}
	*pdu++ = 0x00;
	*pdu++ = GSM_SMS_ENCODE_8BIT;
	*pdu++ = 0xA7;
	*pdu++ = strlen(dat);
	rc += 4;

	while (*dat) {
		*pdu++ = *dat++;
		rc++;
		if (rc > 155) {
			return 0;
		}
	}

	return rc;
}


int SMSEncodePduUCS2(char *pdu, const char *destNum, const char *ucs2, int len) {
	int rc;
	unsigned char ch;
	if (0 == pdu) {
		return 0;
	}
	if (0 == destNum) {
		return 0;
	}
	if (0 == ucs2) {
		return 0;
	}
	if ((*destNum < '0') || (*destNum > '9')) {
		return 0;
	}

	*pdu++ = 0x00;
	*pdu++ = 0x11;
	*pdu++ = 0x00;
	*pdu++ = strlen(destNum);
	if (strncmp(destNum, "86", 2) == 0) {
		*pdu++ = PDU_NUMBER_TYPE_INTERNATIONAL;
	} else {
		*pdu++ = PDU_NUMBER_TYPE_NATIONAL;
	}

	rc = 6;
	while (1) {
		if (0 == *destNum) {
			break;
		}
		ch  = *destNum++ & 0x0F;
		if (0 == *destNum) {
			*pdu++ = 0xF0 | ch;
			break;
		}
		*pdu++ = ((*destNum++) << 4) | ch;
		rc++;
	}
	*pdu++ = 0x00;
	*pdu++ = GSM_SMS_ENCODE_UCS2;
	*pdu++ = 0xA7;
	*pdu++ = len;
	rc += 4;

	memcpy(pdu, ucs2, len);
	rc += len;
	return rc;
}

#if 0
/// 以8bit格式编码PDU.
//  \param out 用于存放编码后的数据;
//  \param destNum 发送的目标号码.
//  \param dat 要编码的数据, 以'\0'结束;
//  \return >0 编码后数据的长度.
//  \return 0 失败.
unsigned char Sms_EncodePdu_8bit(char *pdst, SMSInfo *psms, unsigned char *pdulen) reentrant {
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
	*pdst++ = psms->numberType;
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
	*pdst++ = GSM_SMS_ENCODE_8BIT;
	rc++; 	// DSC
	*pdst++ = 0xA7;
	rc++;				// VP
	*pdst++ = strlen(psms->content);
	rc++;					// UDL
	i = 0;
	while (1) {
		ch0 = psms->content[i++];
		if (ch0 == 0) {
			break;
		}
		*pdst++ = ch0;
		rc++;
	}
	*pdulen = rc - 1;
	return rc;
}
unsigned char Sms_EncodePdu_7bit(char *pdst, SMSInfo *psms, unsigned char *pdulen) reentrant {
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
	*pdst++ = psms->numberType;
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
	*pdst++ = GSM_SMS_ENCODE_7BIT;
	rc++; 	// DSC
	*pdst++ = 0xA7;
	rc++;				// VP
	*pdst++ = strlen(psms->content);
	rc++;				// PID

	i = 0;
	cnt = 0;
	ch0 = 0;
	while (1) {
		if (cnt == 0) {
			ch0 = psms->content[i++];
			if (ch0 == 0) {
				break;
			}
		} else {
			ch0 = ch1 >> cnt;
		}
		ch1 = psms->content[i++];
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
