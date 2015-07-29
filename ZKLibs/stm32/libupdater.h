#ifndef __LIBUPDATER_H__
#define __LIBUPDATER_H__

#include <stdbool.h>

#define UPDATE_VERSION  0x0100

typedef struct {
	unsigned int activeFlag;
	char ftpHost[20];
	char remotePath[40];
	unsigned int ftpPort;
	unsigned int timesFlag[5];
} FirmwareUpdaterMark;

bool FirmwareUpdateSetMark(FirmwareUpdaterMark *tmpMark, const char *host, unsigned short port, const char *remoteFile);

#endif
