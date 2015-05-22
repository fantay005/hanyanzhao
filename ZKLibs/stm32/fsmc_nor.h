/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : fsmc_nor.h
* Author             : MCD Application Team
* Version            : V2.0.3
* Date               : 09/22/2008
* Description        : Header for fsmc_nor.c file.
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FSMC_NOR_H
#define __FSMC_NOR_H

#define Bank1_NOR2_ADDR       ((long)0x64000000)

/* Includes ------------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef struct {
	short Manufacturer_Code;
	short Device_Code1;
	short Device_Code2;
	short Device_Code3;
} NOR_IDTypeDef;

/* NOR Status */
typedef enum {
	NOR_SUCCESS = 0,
	NOR_ONGOING,
	NOR_ERROR,
	NOR_TIMEOUT
} NOR_Status;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void FSMC_NOR_Init(void);
void FSMC_NOR_ReadID(NOR_IDTypeDef *NOR_ID);
NOR_Status FSMC_NOR_EraseSector(long BlockAddr);
NOR_Status FSMC_NOR_EraseBlock(long BlockAddr);
NOR_Status FSMC_NOR_EraseChip(void);
NOR_Status FSMC_NOR_WriteHalfWord(long WriteAddr, short Data);
NOR_Status FSMC_NOR_WriteBuffer(const short *pBuffer, long WriteAddr, long NumHalfwordToWrite);
NOR_Status FSMC_NOR_ProgramBuffer(short *pBuffer, long WriteAddr, long NumHalfwordToWrite);
NOR_Status FSMC_NOR_ReturnToReadMode(void);
NOR_Status FSMC_NOR_Reset(void);
NOR_Status FSMC_NOR_GetStatus(long Timeout);

#define ADDR_SHIFT(A) (Bank1_NOR2_ADDR + (2 * (A)))
#define NOR_WRITE(Address, Data)  (*(unsigned short *)(Address) = (Data))

/******************************************************************************
* Function Name  : FSMC_NOR_ReadHalfWord
* Description    : Reads a half-word from the NOR memory.
* Input          : - ReadAddr : NOR memory internal address to read from.
* Output         : None
* Return         : Half-word read from the NOR memory
*******************************************************************************/
inline short FSMC_NOR_ReadHalfWord(long ReadAddr) {
  NOR_WRITE(ADDR_SHIFT(0x005555), 0x00AA); 
  NOR_WRITE(ADDR_SHIFT(0x002AAA), 0x0055);  
  NOR_WRITE((Bank1_NOR2_ADDR + ReadAddr), 0x00F0 );
	return (*(unsigned short *)((Bank1_NOR2_ADDR + ReadAddr)));
}

/*******************************************************************************
* Function Name  : FSMC_NOR_ReadBuffer
* Description    : Reads a block of data from the FSMC NOR memory.
* Input          : - pBuffer : pointer to the buffer that receives the data read
*                    from the NOR memory.
*                  - ReadAddr : NOR memory internal address to read from.
*                  - NumHalfwordToRead : number of Half word to read.
* Output         : None
* Return         : None
*******************************************************************************/
inline void FSMC_NOR_ReadBuffer(short *pBuffer, long ReadAddr, long NumHalfwordToRead) {
  NOR_WRITE(ADDR_SHIFT(0x05555), 0x00AA);
  NOR_WRITE(ADDR_SHIFT(0x02AAA), 0x0055);
  NOR_WRITE((Bank1_NOR2_ADDR + ReadAddr), 0x00F0);

	for (; NumHalfwordToRead != 0x00; NumHalfwordToRead--) { /* while there is data to read */
		/* Read a Halfword from the NOR */
		*pBuffer++ = *(unsigned short *)((Bank1_NOR2_ADDR + ReadAddr));
		ReadAddr = ReadAddr + 2;
	}
}

#endif /* __FSMC_NOR_H */

/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/
