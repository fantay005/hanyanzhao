#ifndef __XFS__H__
#define	__XFS__H__

typedef struct {
	unsigned char speakTimes;
	unsigned char speakPause;
	unsigned char speakVolume;
	unsigned char speakType;
	unsigned char speakSpeed;
	unsigned char speakTone;
} XFSspeakParam;
void XfsTaskSpeakUCS2(const char *s, int len);
void XfsTaskSpeakGBK(const char *s, int len);
void XfsTaskSetSpeakTimes(int times);
void XfsTaskSetSpeakPause(int sec);
void XfsTaskSetSpeakVolume(char Vol);
void XfsTaskSetSpeakType(char Type);
void XfsTaskSetSpeakTone(char Tone);
void XfsTaskSetSpeakSpeed(char Speed);
void SMS_Prompt(char *p, int len);

#endif
