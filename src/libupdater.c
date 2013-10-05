#include "stm32f10x_flash.h"
#include "stdbool.h"
#include "string.h"
#include "ctype.h"

#define __markSavedAddr 0x0800F800
static const unsigned int __activeFlag = 0xA5A55A5A;

struct UpdateMark {
	unsigned int activeFlag;
	char ftpHost[20];
	char remotePath[40];
	char notifySmsNum[20];
	unsigned int ftpPort;
	unsigned int timesFlag[5];
};



static struct UpdateMark *const __mark = (struct UpdateMark *)__markSavedAddr;
static struct UpdateMark __forProgram;


bool isValidUpdateMark(const struct UpdateMark *mark) {
	int i;
	if ((strlen(mark->ftpHost) < 9) || (strlen(mark->ftpHost) > 17)) {
		return false;
	}

	if ((strlen(mark->remotePath) < 5) || (strlen(mark->remotePath) >= (sizeof(mark->remotePath) - 1))) {
		return false;
	}


	for (i = 0; i < strlen(mark->ftpHost); ++i) {
		if ((!isdigit(mark->ftpHost[i])) && (mark->ftpHost[i] != '.')) {
			return false ;
		}
	}

	return true;
}


void eraseupdatemark(void) {
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	FLASH_ErasePage(0x0800F800);
	FLASH_Lock();
}



bool setFirmwareUpdate(const char *host, unsigned short port, const char *remoteFile, const char *notify) {
	int i;
	unsigned int *pint;

	__forProgram.activeFlag = __activeFlag;
	strncpy(__forProgram.ftpHost, host, sizeof(__forProgram.ftpHost));
	strncpy(__forProgram.remotePath, remoteFile, sizeof(__forProgram.remotePath));
	strncpy(__forProgram.notifySmsNum, notify, sizeof(__forProgram.notifySmsNum));
	__forProgram.ftpPort = port;
	for (i = 0; i < sizeof(__forProgram.timesFlag) / sizeof(__forProgram.timesFlag[0]); ++i) {
		__forProgram.timesFlag[i] = 0xFFFFFFFF;
	}
	if (!isValidUpdateMark(&__forProgram)) {
		return false;
	}

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	FLASH_ErasePage(__markSavedAddr);

	pint = (unsigned int *)&__forProgram;

	for (i = 0; i < sizeof(__forProgram) / sizeof(unsigned int); ++i) {
		FLASH_ProgramWord(__markSavedAddr + i * sizeof(unsigned int), *pint++);
	}

	FLASH_Lock();

	if (__mark->activeFlag != __forProgram.activeFlag) {
		eraseupdatemark();
		return false;
	}

	if (strcmp(__mark->ftpHost, __forProgram.ftpHost)) {
		eraseupdatemark();
		return false;
	}

	if (strcmp(__mark->remotePath, __forProgram.remotePath)) {
		eraseupdatemark();
		return false;
	}

	if (strcmp(__mark->notifySmsNum, __forProgram.notifySmsNum)) {
		eraseupdatemark();
		return false;
	}

	if (__mark->ftpPort != __forProgram.ftpPort) {
		eraseupdatemark();
		return false;
	}

	for (i = 0; i < sizeof(__forProgram.timesFlag) / sizeof(__forProgram.timesFlag[0]); ++i) {
		if (__mark->timesFlag[i] != __forProgram.timesFlag[i]) {
			eraseupdatemark();
			return false;
		}
	}

	return true;
}
