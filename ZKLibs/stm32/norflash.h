#ifndef __NORFLASH_H__
#define __NORFLASH_H__

#include <stdbool.h>
#include <stdint.h>

#include "fsmc_nor.h"


#define XFS_PARAM_STORE_ADDR  (0x400000 - 4*1024)    // LAST 4K
#define GSM_PARAM_STORE_ADDR  (0x400000 - 2*4*1024)
#define FLAG_PARAM_STORE_ADDR (0x400000 - 3*4*1024)
#define USER_PARAM_STORE_ADDR (0x400000 - 4*4*1024)
#define SMS1_PARAM_STORE_ADDR (0x400000 - 5*4*1024)
#define FIX_PARAM_STORE_ADDR  (0x400000 - 6*4*1024)
#define BUSI_PARAM_STORE_ADDR (0x400000 - 7*4*1024)
#define UNICODE_TABLE_ADDR (0x0E0000)
#define UNICODE_TABLE_END_ADDR (UNICODE_TABLE_ADDR + 0x3B2E)
#define GBK_TABLE_OFFSET_FROM_UNICODE (0x3B30)
#define GBK_TABLE_ADDR (UNICODE_TABLE_ADDR + GBK_TABLE_OFFSET_FROM_UNICODE)
#define GBK_TABLE_END_ADDR (UNICODE_TABLE_END_ADDR + GBK_TABLE_OFFSET_FROM_UNICODE)

void NorFlashInit(void);
void NorFlashWrite(uint32_t flash, const short *ram, int len);
void NorFlashRead(uint32_t flash, short *ram, int len);

bool NorFlashMutexLock(uint32_t time);
void NorFlashMutexUnlock(void);

static inline void NorFlashRead2(uint32_t flash, short *ram, int len) {
	FSMC_NOR_ReadBuffer(ram, flash, len);
}

#endif
