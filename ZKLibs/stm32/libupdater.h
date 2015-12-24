#ifndef __LIBUPDATER_H__
#define __LIBUPDATER_H__

#include <stdbool.h>

#define UPDATE_VERSION  0x0100

typedef struct {
	unsigned int activeFlag;
	char ftpHost[20];
	char remotePath[40];
	unsigned int type;
	unsigned int ftpPort;
	unsigned int timesFlag[5];
} FirmwareUpdaterMark;

struct UpdateMark {
	unsigned int RequiredFlag;
	unsigned int SizeOfPAK;
	unsigned int type;
	unsigned int timesFlag[5];
};

bool FirmwareUpdateSetMark(FirmwareUpdaterMark *tmpMark, const char *host, unsigned short port, const char *remoteFile, char type);

#endif
