#ifndef __NORFLASH_H__
#define __NORFLASH_H__

#define XFS_PARAM_STORE_ADDR (0x400000 - 4*1024) // LAST 4K

void NorFlashInit(void);
void NorFlashWrite(long flash, const short *ram, int len);
void NorFlashRead(int flash, short *ram, int len);

#endif
