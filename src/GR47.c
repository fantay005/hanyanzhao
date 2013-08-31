/******************************************************************************/

/******************************************************************************/


/******************************************************************************/
/******************************************************************************/

// 用户信息编码方式
#define GSM_7BIT 0
#define GSM_8BIT 4
#define GSM_UCS2 8
// 短消息参数结构，编码 / 解码共用

const char	PhoneNum[32]; // 目标号码或回复号码 (TP-DA 或 TP-RA)
const char	TP_DCS; // 用户信息编码方式 (TP-DCS)
bit	    	   	message_ture = 0;	//回执确认

/******************************************************************************/

/******************************************************************************/

// Bit7 编码
// pSrc: 源字符串指针
// pDst: 目标编码串指针
// nSrcLength: 源字符串长度
// 返回 : 目标编码串长度
int EncodeBit7(const char	*pSrc,const char *pDst, int nSrcLength) {
	int nSrc; // 源字符串的计数值
	int nDst; // 目标编码串的计数值
	int nChar; // 当前正在处理的组内字符字节的序号，范围是 0-7
	int nLeft; // 上一字节残余的数据
	// 计数值初始化
	nSrc = 0;
	nDst = 0;
	// 将源串每 8 个字节分为一组，压缩成 7 个字节
	// 循环该处理过程，直至源串被处理完
	// 如果分组不到 8 字节，也能正确处理
	while(nSrc<nSrcLength) {
		nChar = nSrc & 7;  // 取源字符串的计数值的最低 3 位
		if(nChar == 0) {   // 处理源串的每个字节
			nLeft = *pSrc; 		 // 组内第一个字节，只是保存起来，待处理下一个字节时使用
		} else {
			*pDst = (*pSrc << (8-nChar)) | nLeft;  	// 组内其它字节，将其右边部分与残余数据相加，得到一个目标编码字节
			nLeft = *pSrc >> nChar; 				// 将该字节剩下的左边部分，作为残余数据保存起来
			pDst++;					// 修改目标串的指针和计数值
			nDst++;
		}
		pSrc++; 	 // 修改源串的指针和计数值
		nSrc++;
	}
	*pDst = nLeft;

	return (nSrcLength - nSrcLength/8); 	 // 返回目标编码串长度
}

/******************************************************************************/
// 7-bit 解码
// pSrc: 源编码串指针
// pDst: 目标字符串指针
// nSrcLength: 源编码串长度
// 返回 : 目标字符串长度
int DecodeBit7(const char	*pSrc,const char	*pDst, int nSrcLength) {
	int nDst; // 目标编码串的计数值
	int nByte; // 当前正在处理的组内字符字节的序号，范围是 0-7
	int nLeft; // 上一字节残余的数据

	// 计数值初始化
	nDst = 0; // 组内字节序号和残余数据初始化
	nByte = 0;
	nLeft = 0;

	// 将源数据每 7 个字节分为一组，解压缩成 8 个字节
	// 循环该处理过程，直至源数据被处理完
	// 如果分组不到 7 字节，也能正确处理
	while(nDst<nSrcLength) {
		*pDst = ((*pSrc << nByte) | nLeft) & 0x7f;    // 将源字节右边部分与残余数据相加，去掉最高位，得到一个目标解码字节
		nLeft = *pSrc >> (7-nByte);  	 // 将该字节剩下的左边部分，作为残余数据保存起来
		pDst++; 			// 修改目标串的指针和计数值
		nDst++;
		nByte++;  			// 修改字节计数值
		if(nByte == 7) {	// 到了一组的最后一个字节
			*pDst = nLeft;	// 额外得到一个目标解码字节
			pDst++; 		// 修改目标串的指针和计数值
			nDst++;
			nByte = 0; 	  	// 组内字节序号和残余数据初始化
			nLeft = 0;
		}
		pSrc++; 		    // 修改源串的指针和计数值
	}
	*pDst = 0;
	return (nSrcLength); 		  // 返回目标串长度
}

/******************************************************************************/
// BetyChange 编码
// pSrc: 源字符串指针
// pDst: 目标编码串指针
// nSrcLength: 源字符串长度
// 返回 : 目标编码串长度
UINT EncodeBetyChange(const char	*pSrc,const char	*pDst, int nSrcLength) {
	int	i;
	UINT	xdata	len;
	int	code	tab[]= {"0123456789ABCDEF"}; // 0x0-0xf 的字符查找表

	for(i=0; i<nSrcLength; i++) {
		*pDst++ = tab[*pSrc >> 4]; // 输出高 4 位
		*pDst++ = tab[*pSrc & 0x0f];// 输出低 4 位
		pSrc++;
	}
	len = ((UINT)nSrcLength) * 2;
	return (len); 	 // 返回目标编码串长度
}

/******************************************************************************/
// BetyChange 解码
// pSrc: 源编码串指针
// pDst: 目标字符串指针
// nSrcLength: 源编码串长度
// 返回 : 目标字符串长度
int	DecodeBetyChange(const char	*pSrc,const char	*pDst, int nSrcLength) {
	int	i;

	for(i=0; i<nSrcLength; i++) {
		if(*pSrc>='0' && *pSrc<='9') {   // 输出高 4 位
			*pDst = (*pSrc - '0') << 4;
		} else {
			*pDst = (*pSrc - 'A' + 10) << 4;
		}
		pSrc++;

		if(*pSrc>='0' && *pSrc<='9') {   // 输出低 4 位
			*pDst |= *pSrc - '0';
		} else {
			*pDst |= *pSrc - 'A' + 10;
		}
		pSrc++;
		pDst++;
	}

	return (nSrcLength); 	   // 返回目标字符串长度
}

/******************************************************************************/
// 正常顺序的字符串转换为两两颠倒的字符串，若长度为奇数，补 'F' 凑成偶数
// 如： "8613851872468" --> "683158812764F8"
// pSrc: 源字符串指针
// pDst: 目标字符串指针
// nSrcLength: 源字符串长度
// 返回 : 目标字符串长度
int InvertNumbers(const char	*pSrc,const char	*pDst,int nSrcLength) {
	int nDstLength; // 目标字符串长度
	int ch,i;

	nDstLength = nSrcLength;

	for(i=0; i<nSrcLength; i+=2) { // 两两颠倒
		ch = *pSrc++; // 保存先出现的字符
		*pDst++ = *pSrc++; // 复制后出现的字符
		*pDst++ = ch; // 复制先出现的字符
	}

	if(nSrcLength & 1) {  // 源串长度是奇数吗？
		*(pDst-2) = 'F'; // 补 'F'
		nDstLength++; // 目标串长度加 1
	}

	return(nDstLength); 	  // 返回目标字符串长度
}

/******************************************************************************/
// 两两颠倒的字符串转换为正常顺序的字符串
// 如： "683158812764F8" --> "8613851872468"
// pSrc: 源字符串指针
// pDst: 目标字符串指针
// nSrcLength: 源字符串长度
// 返回 : 目标字符串长度
int Invert_Return(const char	*pSrc,const char	*pDst,int nSrcLength) {
	int nDstLength; // 目标字符串长度
	int ch,i;

	nDstLength = nSrcLength;

	for(i=0; i<nSrcLength; i+=2) { // 两两颠倒
		ch = *pSrc++; // 保存先出现的字符
		*pDst++ = *pSrc++; // 复制后出现的字符
		*pDst++ = ch; // 复制先出现的字符
	}

	if(*(pDst-1) == 'F') {
		pDst--;
		nDstLength--; // 目标字符串长度减 1
	}

	return(nDstLength); 	  // 返回目标字符串长度
}

/******************************************************************************/

// 以下是 PDU 全串的编解码模块。为简化编程，有些字段用了固定值。
// PDU 编码，用于编制、发送短消息
// pSrc: 源 PDU 参数指针
// pDst: 目标 PDU 串指针
// 返回 : 目标 PDU 串长度
UINT gsmEncodePdu(const char	*pSrc,const char	*pDst) {
	UINT 	xdata		nDstLength; // 目标 PDU 串长度
	int 	xdata		code_char[160];	  //编码字符缓冲区
	const char 		buf[5]; // 内部用的缓冲区
	const char 		m;

	buf[0] = 0x00;								 //默认中心号码
	nDstLength = EncodeBetyChange(buf, pDst, 1); // 转换 1 个字节到目标 PDU 串

	if(message_ture == 1)
		buf[0] = 0x31; //需要回执
	else
		buf[0] = 0x11;	  // 是发送短信 (TP-MTI=01) ， TP-VP 用相对格式 (TP-VPF=10)

	buf[1] = 0x00; // TP-MR=0
	m = strlen(PhoneNum);
	buf[2] = m; // 目标地址数字个数 (TP-DA 地址字符串真实长度 )
	if((PhoneNum[0] == '8')&&(PhoneNum[1] == '6'))
		buf[3] = 0x91; //用国际格式号码
	else
		buf[3] = 0x81; //用国内格式号码

	nDstLength += EncodeBetyChange(buf, pDst + nDstLength, 4); // 转换 4 个字节到目标 PDU 串

	nDstLength += InvertNumbers(PhoneNum, pDst + nDstLength, m); // 转换 TP-DA 到目标 PDU 串

	// TPDU 段协议标识、编码方式、用户信息等
	buf[0] = 0; // 协议标识 (TP-PID)
	buf[1] = TP_DCS; // 用户信息编码方式 (TP-DCS)
	buf[2] = 255; // 有效期 (TP-VP)=0 为 5 分钟 ; =255为最长。
	buf[3] = strlen(pSrc);  //信息长度

	if(data_gsm == 1)
		buf[3] = data_len;

	if(buf[1] == GSM_7BIT) {
		buf[4] = EncodeBit7(pSrc, code_char, buf[3]);
	} else	if(buf[1] == GSM_8BIT) {
		buf[4] = buf[3];
		memcpy(code_char, pSrc, buf[3]);
	}

	nDstLength += EncodeBetyChange(buf, pDst + nDstLength, 4); // 转换 4 个字节到目标 PDU 串

	nDstLength += EncodeBetyChange(code_char, pDst + nDstLength, buf[4]); // 转换信息内容到目标 PDU 串

	*(pDst + nDstLength) = 0x1a; // 以 Ctrl-Z 结束
	nDstLength++;

	return (nDstLength); 		   // 返回目标字符串长度
}

/******************************************************************************/

// PDU 解码，用于接收、阅读短消息
// pSrc: 源 PDU 串指针
// pDst: 目标 PDU 参数指针
// 返回 : 用户信息串长度
int gsmDecodePdu(const char	*pSrc,const char	*pDst) {
	int 	nDstLength; // 目标 PDU 串长度
	int 	xdata	code_char[165];	  //解码字符缓冲区
	int 	code	receive_ture[] = {"ok"};	  //解码字符缓冲区
	const char	i,m;

	m=(*(pSrc+1))&0xf; // 取SMSC 号码串长度
	pSrc += m*2 + 2;   // 指针后移

	DecodeBetyChange(pSrc,&m,1);		  // 基本参数
	pSrc += 2; // 指针后移

	if((m & 0x02) == 0x02) {	//短信接受确认信息
		pSrc += 2;

		DecodeBetyChange(pSrc, &m, 1);	// 取长度
		pSrc += 4;			// 指针后移，忽略了回复地址(TP-RA)格式

		nDstLength = Invert_Return(pSrc,pDst,(m+1)&0xfe);	 // 取 TP-RA 号码
		pSrc +=	(m+1)&0xfe;		// 指针后移

		*(pDst+nDstLength) = ',';
		nDstLength++;

		pSrc += 14;
		nDstLength += Invert_Return(pSrc,pDst+nDstLength,12);	// 服务时间戳字符串

		*(pDst+nDstLength) = ',';
		nDstLength++;

		memcpy(pDst+nDstLength, receive_ture, 2);
		nDstLength += 2;

		return  (nDstLength);
	}
	//08 91683108505905F0 06 1D 0D 91683158703261F6 505021908480 23 505021908411 23 00 FF  //回执PDU

	//08 91683108200505F0 24    0D 91683158714209F8 00 00 400152803535 00 04 D4F29C0E	   //接受PDU

	DecodeBetyChange(pSrc,code_char,1);
	m = code_char[0];						//回复地址信息长度 (TP-UDL)

	pSrc += 4; // 指针后移

	nDstLength = Invert_Return(pSrc,pDst,(m+1)&0xfe);	 // 取 TP-RA 号码
	pSrc +=	(m+1)&0xfe;

	*(pDst+nDstLength) = ',';
	nDstLength++;

	pSrc += 2;

	TP_DCS = *(pSrc+1) & 0x0f; 								   //编码方式
	pSrc += 2;

	nDstLength += Invert_Return(pSrc,pDst+nDstLength,12);	// 服务时间戳字符串
	pSrc += 14;

	*(pDst+nDstLength) = ',';
	nDstLength++;

	DecodeBetyChange(pSrc,code_char,1);
	m = code_char[0];						// 用户信息长度 (TP-UDL)
	pSrc += 2;

	DecodeBetyChange(pSrc,code_char,m);					// 格式转换

	if(TP_DCS == GSM_7BIT) {
		nDstLength += DecodeBit7(code_char,pDst+nDstLength,m);				// 7-bit 解码
	} else	if(TP_DCS == GSM_8BIT) {
		memcpy(pDst+nDstLength, code_char, m); 					   	// 8-bit 解码
		nDstLength += m;
	} else	if(TP_DCS == GSM_UCS2) {
		for(i = 0; i < m;  ) {
			*(pDst+nDstLength) = *(code_char + i + 1); 					   	// 16-bit 解码
			nDstLength ++;
			i += 2;
		}
	}

	*(pDst+nDstLength) = '\n';
	nDstLength++;
	*(pDst+nDstLength) = '\0';
	nDstLength++;

	return (nDstLength);
}

/******************************************************************************/

// 发送短消息
// pSrc: 源 PDU 参数指针
bit	SendMessage(const char	*pSrc) {
	UINT  xdata	PduLength; // PDU 串长度
	int xdata	SmsLength; // SMS 串长度
	UINT  xdata	r_len;	 // 串口收到的数据长度
	int m,i,j;
	int code	cmd0[9]= {"AT+CMGS="}; // 命令串
	int xdata	cmd[9]; // 命令串
	int xdata	pdu[500]; // PDU 串
	int xdata ans[64]; // 应答串

	while(gsm_busy)
		os_wait(K_TMO,1,0);

	gsm_busy = 1;

	PduLength = gsmEncodePdu(pSrc, pdu); // 根据 PDU 参数，编码 PDU 串

	SmsLength = (PduLength - 2)/2; // 取 PDU 串中的 SMS 信息长度

	for(i=0; i<8; i++)
		cmd[i]=cmd0[i];

	get_string_clear1( );				//清空缓冲区

	put_string1(cmd, 8);

	i = SmsLength/100;
	if(i != 0)
		put_char1(i +0x30);		   //发送指令

	SmsLength = SmsLength%100;
	m = SmsLength/10;
	if((m != 0)||(i != 0))
		put_char1(m +0x30);		   //发送PDU长度

	put_char1((SmsLength%10)+0x30);
	put_char1(0x0d);
	put_char1(0x0a);

	for(i = 2; i>0; i--) {
		r_len = get_string1(ans);	 // 读应答数据
		if(r_len != 0) {
			break;
		}
	}
	if(r_len == 0) {
		gsm_busy = 0;
		return(FALSE);
	}

	if(ans[r_len-2] != '>') {
		gsm_busy = 0;
		return(FALSE);
	}
	put_string1(pdu,PduLength);		//发送PDU

	for(j=0; j<4; j++) {
		for(i=0; i<25; i++) {
			memset(ans,0x00,64);
			r_len = get_string1(ans);	 // 读应答数据
			if(r_len > 0) {
				break;
			}
		}

		ans[r_len] = '\0';

		if((r_len > 3) && (strstr(ans,"OK") != NULL)) {	 //查找OK
			gsm_busy = 0;
			day_inc = 0;
			return(TRUE);
		}
	}
	gsm_busy = 0;
	return(FALSE);
}

/******************************************************************************/
// 删除短消息
// index: 短消息序号，从 1 开始
void DeleteMessage(int index) {
	int m,i;
	int code	cmd0[9]= {"AT+CMGD="}; // 命令串
	int xdata	cmd[9]; 			// 命令串

	for(i=0; i<8; i++)
		cmd[i]=cmd0[i];

	get_string_clear1( );				//清空缓冲区

	put_string1(cmd, 8);
	m = index/10;
	if(m != 0)
		put_char1(m +0x30);
	put_char1((index%10)+0x30);
	put_char1(0x0d);
	put_char1(0x0a);

	get_string_clear1( );				//清空缓冲区
}

/******************************************************************************/

// 读取短消息
// 用CMGL
//一次读出一条短消息
// pDrc 返回字符串指针
// index 短信序号
//返回：字符串长度
int ReadMessage(const char	*pDrc) {
	int m,i,index;
	UINT  xdata	r_len; 		 // 串口收到的数据长度
	int code	cmd0[16]= {"AT+CMGL=4\r\n"}; // 命令串
	int xdata	cmd[16]; 			// 命令串
	int xdata	buf[500]; // 接受缓冲
	int xdata	*ptr;

	while(gsm_busy)
		os_wait(K_TMO,1,0);

	ptr = buf;
	gsm_busy = 1;

	for(i=0; i<12; i++)
		cmd[i]=cmd0[i];

	get_string_clear1( );				//清空缓冲区

	put_string1(cmd, 11);

	for(i = 8; i>0; i--) {
		r_len = get_string1(buf);	 // 读应答数据
		if(r_len > 30)
			break;
	}

	if(r_len < 30) {
		gsm_busy = 0;
		return(0);
	}

	buf[r_len] = '\0';

	if((ptr = strstr(buf, "CMGL:")) != NULL) {
		ptr = strchr(buf,',');		// 跳过"+CMGL:"
		index = ((*(ptr-2))&0x0f)*10 + ((*(ptr-1))&0x0f);	// 读取序号

		ptr = strstr(ptr, "\r\n");	// 找下一行
		ptr += 2;		// 跳过"\r\n"

		m = gsmDecodePdu(ptr, pDrc);	// PDU串解码

		DeleteMessage(index);

		gsm_busy = 0;
		return(m);
	}
	gsm_busy = 0;
	return (0);
}

/******************************************************************************/
void read_storage(int	n) {
	int code	cmd0[]= {"AT+CPMS= ME , SM , SM \r\n"}; // 命令串
	int code	cmd1[]= {"AT+CPMS= SM , SM , SM \r\n"}; // 命令串
	int xdata	cmd[25]; 			// 命令串
	int i;

	while(gsm_busy)
		os_wait(K_TMO,1,0);
	gsm_busy = 1;

	if(n == 1) {
		for(i=0; i<24; i++)
			cmd[i]=cmd1[i];
	} else {
		for(i=0; i<24; i++)
			cmd[i]=cmd0[i];
	}
	cmd[8] = '"';
	cmd[11] = '"';
	cmd[13] = '"';
	cmd[16] = '"';
	cmd[18] = '"';
	cmd[21] = '"';
	get_string_clear1( );				//清空接收缓冲区
	put_string1(cmd, 24);   				//串口接收回复，关
	os_wait(K_TMO,10,0);
	get_string_clear1( );				//清空接收缓冲区

	gsm_busy = 0;
}

/******************************************************************************
// 读取电话本
// 用CPBR
//一次读出一条电话号码
// pDrc 返回字符串指针
// index 电话号码序号
//返回：字符串长度
int read_phonebook(const char	*pDrc,int	index)
{
	int m,i;
	int r_len; 		 // 串口收到的数据长度
	int code	cmd0[]={"AT+CPBR="}; // 命令串
	int code	cmd1[]={"AT+CPBS=ME\r\n"}; // 命令串
	int xdata	cmd[16]; 			// 命令串
	int xdata	buf[128]; // 接受缓冲
	int xdata	*ptr;

   	while(gsm_busy)
		os_wait(K_TMO,1,0);
	gsm_busy = 1;

	for(i=0;i<12;i++)
		cmd[i]=cmd1[i];

 	get_string1(buf);				//清空发送缓冲区和接收缓冲区
	put_string1(cmd, 12);

	os_wait(K_TMO,10,0);

	for(i=0;i<8;i++)
		cmd[i]=cmd0[i];

 	get_string1(buf);				//清空发送缓冲区和接收缓冲区
	put_string1(cmd, 8);

	m = index/10;
	if(m != 0)
		put_char1(m +0x30);
	put_char1((index%10)+0x30);
	put_char1(0x0d);
	put_char1(0x0a);

	for(i = 2; i>0; i--)
	{
		r_len = get_string1(buf);	 // 读应答数据
		if(r_len >15)
			break;
	}
	if(r_len <15)
	{
		gsm_busy = 0;
		return(0);
	}

	buf[127] = '\0';
	if((ptr = strchr(buf,'"')) != NULL)
	{
			ptr++;
			r_len = 0;
			m = *ptr;

			while(m != '"')
			{
				m = *ptr++;
				*pDrc++ = m;

			   	r_len++;
			}
			gsm_busy = 0;
			return(r_len-1);
	}
	gsm_busy = 0;
	return (0);
}

/******************************************************************************
// 写电话本
// 用CPBW
//一次写一条电话号码
//pSrc: 源字符串指针
//index 电话号码序号  	turn(TRUE); return(FALSE);
//len 电话号码长度
//返回：成功/失败  		AT+CPBW=4,13459553766,129,CENTER
bit write_phonebook(const char	*pSrc,int	len,int	index)
{
	int m,i,j;
	int r_len; 		 // 串口收到的数据长度
	int code	cmd0[]={"AT+CPBW="}; // 命令串
	int code	cmd0_[]={",129,strong\r\n"}; // 命令串
	int code	cmd1[]={"AT+CPBS=ME\r\n"}; // 命令串
	int xdata	cmd[16]; 			// 命令串
	int xdata	buf[128]; // 接受缓冲
	int xdata	*ptr;

   	while(gsm_busy)
		os_wait(K_TMO,1,0);
	gsm_busy = 1;

	for(i=0;i<12;i++)
		cmd[i]=cmd1[i];

 	get_string1(buf);				//清空发送缓冲区和接收缓冲区
	put_string1(cmd, 12);

	os_wait(K_TMO,10,0);

	for(i=0;i<8;i++)
		cmd[i]=cmd0[i];

 	get_string1(buf);				//清空发送缓冲区和接收缓冲区
	put_string1(cmd, 8);

	m = index/10;
	if(m != 0)
		put_char1(m +0x30);
	put_char1((index%10)+0x30);
	put_char1(',');

	put_string1(pSrc, len);

	for(i=0;i<13;i++)
		cmd[i]=cmd0_[i];
	put_string1(cmd, 13);

	os_wait(K_TMO,1,0);

	for(j = 2; j>0; j--)
	{
		for(i = 2; i>0; i--)
		{
			r_len = get_string1(buf);	 // 读应答数据
			if(r_len > 0)
				break;
		}
		if(r_len == 0)
		{
			gsm_busy = 0;
			return(FALSE);
		}

		buf[127] = '\0';
		if((ptr = strstr(buf,"OK")) != NULL)
		{
			gsm_busy = 0;
			return(TRUE);
		}
	}

	gsm_busy = 0;
	return (FALSE);
}

/******************************************************************************/
bit  ReadTime(void) {
	UINT  xdata	r_len; 		 // 收到的数据长度
	int 	i;
	int code	cmd0[ ]= {"AT+CCLK?\r\n"}; // 命令串
	int xdata	cmd[11]; 			// 命令串
	int xdata	buf[50]; // 接受缓冲
	int xdata	*ptr;
	int xdata  *pDrc;

	while(gsm_busy)
		os_wait(K_TMO,1,0);

	ptr = buf;
	pDrc = time;
	gsm_busy = 1;

	for(i=0; i<10; i++)
		cmd[i]=cmd0[i];

	get_string_clear1( );				//清空缓冲区

	put_string1(cmd, 10);

	for(i = 2; i>0; i--) {
		r_len = get_string1(buf);	 // 读应答数据
		if(r_len > 12)
			break;
	}

	if(r_len < 12) {
		gsm_busy = 0;
		return(FALSE);
	}

	buf[127] = '\0';
	if((ptr = strstr(buf, "CCLK:")) != NULL) {
		ptr += 7;		// 跳过(+CCLK: ")

		for(i=0; i<6; i++) {
			*pDrc = ((*ptr&0x0f)<<4) + (*(ptr+1)&0x0f);
			pDrc++;
			ptr +=3;
		}

		gsm_busy = 0;
		return(TRUE);
	}

	gsm_busy = 0;
	return(FALSE);
}

/******************************************************************************/
bit		TimeCompare_(const char	*sp1,const char	*sp2) {
	if(*sp1>*sp2)	return(1);
	if(*sp1<*sp2)	return(0);
	sp1++;
	sp2++;

	if(*sp1>*sp2)	return(1);
	if(*sp1<*sp2)	return(0);
	sp1++;
	sp2++;

	if(*sp1>*sp2)	return(1);
	if(*sp1<*sp2)	return(0);
	sp1++;
	sp2++;

	if(*sp1>*sp2)	return(1);
	if(*sp1<*sp2)	return(0);
	sp1++;
	sp2++;

	if(*sp1>*sp2)	return(1);
	if(*sp1<*sp2)	return(0);
	sp1++;
	sp2++;

	if(*sp1>*sp2)	return(1);
	if(*sp1<*sp2)	return(0);

	return(0);
}
/******************************************************************************/
void  SetupTime(int xdata  *pSrc) {
	int xdata	buf1[33];
	int code	cmd[ ]= {"AT+CCLK= 00/00/00,00:00:00+00 \r\n"}; // 命令串
	int xdata	*sp;

	if((sp = strchr(pSrc,',')) != NULL) {
		sp++;
		buf1[0] = (((*sp) & 0x0f)<<4) + (*(sp+1) & 0x0f);
		sp += 2;
		buf1[1] = (((*sp) & 0x0f)<<4) + (*(sp+1) & 0x0f);
		sp += 2;
		buf1[2] = (((*sp) & 0x0f)<<4) + (*(sp+1) & 0x0f);
		sp += 2;
		buf1[3] = (((*sp) & 0x0f)<<4) + (*(sp+1) & 0x0f);
		sp += 2;
		buf1[4] = (((*sp) & 0x0f)<<4) + (*(sp+1) & 0x0f);
		sp += 2;
		buf1[5] = (((*sp) & 0x0f)<<4) + (*(sp+1) & 0x0f);

		if((TimeCompare_(buf1,time) == 1)||((memcmp(time,buf1,4) == 0)&&((time[4]-buf1[4]) == 1))) {
			memcpy(buf1,cmd,32);
			sp = strchr(pSrc,',') + 1;

			buf1[8] = '"';

			buf1[9]  = *sp++;
			buf1[10] = *sp++;

			buf1[12] = *sp++;
			buf1[13] = *sp++;

			buf1[15] = *sp++;
			buf1[16] = *sp++;

			buf1[18] = *sp++;
			buf1[19] = *sp++;

			buf1[21] = *sp++;
			buf1[22] = *sp++;

			buf1[24] = *sp++;
			buf1[25] = *sp++;

			buf1[29] = '"';

			while(gsm_busy)
				os_wait(K_TMO,1,0);

			gsm_busy = 1;

			get_string1(buf1);

			put_string1(buf1, 32);

			get_string1(buf1);				//清空接收缓冲区

			gsm_busy = 0;
		}
	}
}

/******************************************************************************/
void  gsm_status_init(void) {
	int 	i;
	int code	cmd0[ ]= {"AT+CMGF=0\r\n"}; 		// 命令串 :
	int code	cmd1[ ]= {"ATE0\r\n"}; 		       	// 命令串 : 串口接收回复，关
	int code	cmd2[ ]= {"AT+CNMI=3,1,0,2,0\r\n"}; 	// 命令串 : 短信提示，开
	int xdata	cmd[20]; 			// 命令串

	while(gsm_busy)
		os_wait(K_TMO,1,0);
	gsm_busy = 1;

	for(i=0; i<11; i++)
		cmd[i]=cmd0[i];
	get_string_clear1( );				//清空接收缓冲区
	put_string1(cmd, 11);   			//

	os_wait(K_TMO,100,0);

	for(i=0; i<6; i++)
		cmd[i]=cmd1[i];
	get_string_clear1( );				//清空接收缓冲区
	put_string1(cmd, 6);   				//串口接收回复，关

	os_wait(K_TMO,100,0);

	for(i=0; i<19; i++)
		cmd[i]=cmd2[i];
	get_string_clear1( );				//清空接收缓冲区
	put_string1(cmd, 19);   			//短信提示，开

	os_wait(K_TMO,10,0);
	get_string_clear1( );				//清空接收缓冲区

	gsm_busy = 0;
}

/******************************************************************************/
void  gsm_init(void) {
	gsm_igt_0( );

	os_wait( K_TMO, 250, 0);

	gsm_igt_1( );

	TP_DCS = GSM_8BIT;
}

/******************************************************************************/
void	gsm_power_down(void) {
	int 	i;
	int code	cmd0[ ]= {"AT+CFUN=0\r\n"}; 		// 命令串 : power down
	int xdata	cmd[11]; 			// 命令串

	while(gsm_busy)
		os_wait(K_TMO,1,0);
	gsm_busy = 1;

	for(i=0; i<11; i++)
		cmd[i]=cmd0[i];
	put_string1(cmd,11);   			// power down

	os_wait(K_TMO,250,0);
	os_wait(K_TMO,250,0);
	os_wait(K_TMO,250,0);
	os_wait(K_TMO,250,0);
	get_string_clear1( );				//清空接收缓冲区

	gsm_busy = 0;
}

/******************************************************************************/
bit	gprs_init(void) {
	int 	i,r_len;
	int xdata	buf[30];

	get_string_clear1( );				//清空接收缓冲区

	put_string1("at*e2ipa=1,1\r\n",14);   // 激活GR47 PDP连接

	for(i = 50; i>0; i--) {		  //10s
		r_len = get_string1(buf);	 // 读应答数据
		buf[r_len] = '\0';
		if(r_len > 2)
			break;
	}
	if(strstr(buf,"OK") != NULL) {
		return(TRUE);
	}
	return(FALSE);
}

/******************************************************************************/
void	cmd_deal_gprs(int xdata *);
/******************************************************************************/
bit	gprs_connect(void) {
	int 	i,r_len;
	int xdata	buf[400];

	get_string_clear1( );				//清空接收缓冲区

	put_string1("at*e2ipo?\r\n",11);   // 连接？

	for(i = 10; i>0; i--) {			  //2s
		r_len = get_string1(buf);	 // 读应答数据
		buf[r_len] = '\0';
		if(r_len > 6)
			break;
	}
	if(strchr(buf,'1') != NULL) { //已经连接
		get_string_clear1( );				//清空接收缓冲区
		put_string1("ATO\r\n",5);
		for(i = 10; i>0; i--) {			  //2s
			r_len = get_string1(buf);	 // 读应答数据
			buf[r_len] = '\0';
			if(r_len > 6)
				break;
		}
		cmd_deal_gprs(buf);
		return(TRUE);
	}

	put_string1("at*e2ipo=1,",11);   // 连接
	put_char1('"');
	put_string1(gprs_ip,strlen(gprs_ip));   	// ip
	put_char1('"');
	put_char1(',');
	put_string1(gprs_port,strlen(gprs_port));	//port
	put_string1("\r\n",2);

	for(i = 50; i>0; i--) {	  //10s
		r_len = get_string1(buf);	 // 读应答数据
		buf[r_len] = '\0';
		if(r_len > 6)
			break;
	}
	if(strstr(buf,"CONNECT") != NULL) { //成功
		return(TRUE);
	}
	return(FALSE);
}

/******************************************************************************/
void	gprs_close(void) {
	gprs_dtr = 0;
	os_wait(K_TMO,30,0);
	gprs_dtr = 1;
}

/******************************************************************************/
bit	gprs_send(const char *sp) {
	int 	m,r_len;
	int xdata	buf[400];

	while(gsm_busy)
		os_wait(K_TMO,1,0);
	gsm_busy = 1;

	m = 5;
	while( gprs_init( )==FALSE ) {
		gprs_close( );
		m--;
		if(m == 0) {
			gsm_busy = 0;
			return(FALSE);
		}
	}
	m = 5;
	while( gprs_connect( )==FALSE ) {
		gprs_close( );
		m--;
		if(m == 0) {
			gsm_busy = 0;
			return(FALSE);
		}
	}
	put_string1(sp,strlen(sp));

	for(m = 150; m>0; m--) {	  //30s
		r_len = get_string1(buf);	 // 读应答数据
		buf[r_len] = '\0';
		if(r_len > 1)
			break;
	}
	if(strstr(buf,"OK") != NULL) { //成功
		gprs_close( );
		gsm_busy = 0;
		return(TRUE);
	}
	gprs_close( );
	gsm_busy = 0;
	return(FALSE);
}

/******************************************************************************/
void	gprs_rcv(void) {
	int 	i,r_len;
	int xdata	buf[400];

	while(gsm_busy)
		os_wait(K_TMO,1,0);
	gsm_busy = 1;

	get_string_clear1( );				//清空接收缓冲区
	put_string1("at*e2ipo?\r\n",11);   // 连接？

	for(i = 10; i>0; i--) {			  //2s
		r_len = get_string1(buf);	 // 读应答数据
		buf[r_len] = '\0';
		if(r_len > 6)
			break;
	}
	if(strchr(buf,'1') != NULL) { //已经连接
		get_string_clear1( );				//清空接收缓冲区
		put_string1("ATO\r\n",5);
		for(i = 10; i>0; i--) {			  //2s
			r_len = get_string1(buf);	 // 读应答数据
			buf[r_len] = '\0';
			if(r_len > 6)
				break;
		}
		cmd_deal_gprs(buf);
		os_wait(K_TMO,50,0);
		gprs_close( );
	}
	gsm_busy = 0;
}

/******************************************************************************/

