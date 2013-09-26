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

typedef struct __sms_t {
	unsigned char number_type;
	unsigned char encode_type;
	unsigned char content_len;
	char number[15];
	char time[15];
	char sms_content[162];
} sms_t;

#define SMS_SAVE_FLAG_OFFSET (1+1)
#define SMS_READFLAG_OFFSET (1)
#define SMS_CONTENT_OFFSET (1+1+4+16+15)

void SMSDecodePdu(const char *pdu, sms_t *psms);
int SMSEncodePdu8bit(char *out, char *destNum, const char *dat);
int SMSEncodePduUCS2(char *out, char *destNum, const char *ucs2, int len);

#endif // ifndef __SMS_H__


