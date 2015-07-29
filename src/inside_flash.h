#ifndef __INSIDE_FLASH_H__
#define __INSIDE_FLASH_H__

#include <stdint.h>

void STMFLASH_Write(uint32_t WriteAddr, uint16_t *pBuffer, uint16_t NumToWrite);

#endif
