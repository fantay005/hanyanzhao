#ifndef __XFS__H__
#define	__XFS__H__

void XfsTaskSpeakUCS2(const char *s, int len);
void XfsTaskSpeakGBK(const char *s, int len);
void XfsTaskSetSpeakTimes(int times);
unsigned char xfsChangePara(unsigned char type, unsigned char para);
void XfsTaskSetSpeakPause(int sec);

#endif
