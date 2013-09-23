#ifndef __NORFLASH_H__
#define __NORFLASH_H__

#include <stdbool.h>

#include "fsmc_nor.h"

#define XFS_PARAM_STORE_ADDR (0x400000 - 4*1024) // LAST 4K

void NorFlashInit(void);
void NorFlashWrite(long flash, const short *ram, int len);
void NorFlashRead(int flash, short *ram, int len);

bool NorFlashMutexLock(int time);
void NorFlashMutexUnlock(void);

static inline void NorFlashRead2(int flash, short *ram, int len) {
	FSMC_NOR_ReadBuffer(ram, flash, len / 2);
}

#endif
