#include <ctype.h>
#include <string.h>
#include "stm32f10x_flash.h"
#include "libupdater.h"

#define __firmwareUpdaterInternalFlashMarkSavedAddr 0x0800F800
const unsigned int __firmwareUpdaterActiveFlag = 0xA5A55A5A;
FirmwareUpdaterMark *const __firmwareUpdaterInternalFlashMark = (FirmwareUpdaterMark *)__firmwareUpdaterInternalFlashMarkSavedAddr;

bool FirmwareUpdaterIsValidMark(const FirmwareUpdaterMark *mark) {
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

void FirmwareUpdaterEraseMark(void) {
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	FLASH_ErasePage(0x0800F800);
	FLASH_Lock();
}

bool FirmwareUpdateSetMark(FirmwareUpdaterMark *tmpMark, const char *host, unsigned short port, const char *remoteFile, char type) {
	int i;
	unsigned int *pint;

	tmpMark->activeFlag = __firmwareUpdaterActiveFlag;
	strncpy(tmpMark->ftpHost, host, sizeof(tmpMark->ftpHost));
	strncpy(tmpMark->remotePath, remoteFile, sizeof(tmpMark->remotePath));
	tmpMark->ftpPort = port;
	for (i = 0; i < sizeof(tmpMark->timesFlag) / sizeof(tmpMark->timesFlag[0]); ++i) {
		tmpMark->timesFlag[i] = 0xFFFFFFFF;
	}
	tmpMark->type = type;
	
	if (!FirmwareUpdaterIsValidMark(tmpMark)) {
		return false;
	}

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	FLASH_ErasePage(__firmwareUpdaterInternalFlashMarkSavedAddr);

	pint = (unsigned int *)tmpMark;

	for (i = 0; i < sizeof(FirmwareUpdaterMark) / sizeof(unsigned int); ++i) {
		FLASH_ProgramWord(__firmwareUpdaterInternalFlashMarkSavedAddr + i * sizeof(unsigned int), *pint++);
	}

	FLASH_Lock();

	if (__firmwareUpdaterInternalFlashMark->activeFlag != tmpMark->activeFlag) {
		FirmwareUpdaterEraseMark();
		return false;
	}

	if (strcmp(__firmwareUpdaterInternalFlashMark->ftpHost, tmpMark->ftpHost)) {
		FirmwareUpdaterEraseMark();
		return false;
	}

	if (strcmp(__firmwareUpdaterInternalFlashMark->remotePath, tmpMark->remotePath)) {
		FirmwareUpdaterEraseMark();
		return false;
	}

	if (__firmwareUpdaterInternalFlashMark->ftpPort != tmpMark->ftpPort) {
		FirmwareUpdaterEraseMark();
		return false;
	}
	
	if (__firmwareUpdaterInternalFlashMark->type != tmpMark->type) {
		FirmwareUpdaterEraseMark();
		return false;
	}

	for (i = 0; i < sizeof(tmpMark->timesFlag) / sizeof(tmpMark->timesFlag[0]); ++i) {
		if (__firmwareUpdaterInternalFlashMark->timesFlag[i] != tmpMark->timesFlag[i]) {
			FirmwareUpdaterEraseMark();
			return false;
		}
	}

	return true;
}
