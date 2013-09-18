#ifndef __LIBUPDATER_H__
#define __LIBUPDATER_H__

typedef struct {
	unsigned int activeFlag;
	char ftpHost[20];
	char remotePath[40];
	char notifySmsNum[20];
	unsigned int ftpPort;
	unsigned int timesFlag[5];
} FirmwareUpdaterMark;


#endif
