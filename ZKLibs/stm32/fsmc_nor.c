#include "stm32f10x_fsmc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_rcc.h"
#include "fsmc_nor.h"

#define BlockErase_Timeout    ((long)0x00A00000)
#define ChipErase_Timeout     ((long)0x30000000)
#define Program_Timeout       ((long)0x00001400)

/*******************************************************************************
* Function Name  : FSMC_NOR_Init
* Description    : Configures the FSMC and GPIOs to interface with the NOR memory.
*                  This function must be called before any write/read operation
*                  on the NOR.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void FSMC_NOR_Init(void) {
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  p;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE |
						   RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG, ENABLE);

	/*-- GPIO Configuration ------------------------------------------------------*/
	/* NOR Data lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 |
								  GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
								  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 |
								  GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOR Address lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |   //A0,A1,A2,A3,A4,A5,
								  GPIO_Pin_4 | GPIO_Pin_5 |
								  GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;//A6,A7,A8,A9
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |    //A10,A11,A12,A13,A14,A15,
								  GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;  //A16,A17,A18
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	//GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;  //A19,A20,   A21,A22
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;  //A19,A20,   A21  地址线没用那么大  wang2012-6-14
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOE and NWE configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NE2 configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	/*-- FSMC Configuration ----------------------------------------------------*/
	p.FSMC_AddressSetupTime = 0x05;
	p.FSMC_AddressHoldTime = 0x00;
	p.FSMC_DataSetupTime = 0x07;
	p.FSMC_BusTurnAroundDuration = 0x00;
	p.FSMC_CLKDivision = 0x00;
	p.FSMC_DataLatency = 0x00;
	p.FSMC_AccessMode = FSMC_AccessMode_B;

	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM2;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_NOR;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

	/* Enable FSMC Bank1_NOR Bank */
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM2, ENABLE);
	FSMC_NOR_Reset();
}

/******************************************************************************
* Function Name  : FSMC_NOR_ReadID
* Description    : Reads NOR memory's Manufacturer and Device Code.
* Input          : - NOR_ID: pointer to a NOR_IDTypeDef structure which will hold
*                    the Manufacturer and Device Code.
* Output         : None
* Return         : None
*******************************************************************************/
void FSMC_NOR_ReadID(NOR_IDTypeDef *NOR_ID) {
	NOR_WRITE(ADDR_SHIFT(0x05555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AAA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x05555), 0x0090);

	NOR_ID->Manufacturer_Code = *(unsigned short *) ADDR_SHIFT(0x0000);
	NOR_ID->Device_Code1 = *(unsigned short *) ADDR_SHIFT(0x0001);
//	NOR_ID->Device_Code2 = *(unsigned short *) ADDR_SHIFT(0x000E);
//	NOR_ID->Device_Code3 = *(unsigned short *) ADDR_SHIFT(0x000F);
	FSMC_NOR_ReturnToReadMode();
}
/*******************************************************************************
* Function Name  : FSMC_NOR_EraseSector  擦除一个扇区
* Description    : Erases the specified Nor memory Sector.
* Input          : - BlockAddr: address of the block to erase.
* Output         : None
* Return         : NOR_Status:The returned value can be: NOR_SUCCESS, NOR_ERROR
*                  or NOR_TIMEOUT
*******************************************************************************/
NOR_Status FSMC_NOR_EraseSector(long BlockAddr) {
	NOR_Status status;
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x2AAA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x2AAA), 0x0055);
	NOR_WRITE((Bank1_NOR2_ADDR + BlockAddr), 0x30);			   //39VF3210B是0x50

	status = FSMC_NOR_GetStatus(BlockErase_Timeout);
	FSMC_NOR_ReturnToReadMode();
	return status;
}

/*******************************************************************************
* Function Name  : FSMC_NOR_EraseBlock  擦除一个块
* Description    : Erases the specified Nor memory block.
* Input          : - BlockAddr: address of the block to erase.
* Output         : None
* Return         : NOR_Status:The returned value can be: NOR_SUCCESS, NOR_ERROR
*                  or NOR_TIMEOUT
*******************************************************************************/
NOR_Status FSMC_NOR_EraseBlock(long BlockAddr) {
	NOR_Status status;
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x2AAA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x2AAA), 0x0055);
	NOR_WRITE((Bank1_NOR2_ADDR + BlockAddr), 0x50);		   //39VF3201b是0x30


	status = FSMC_NOR_GetStatus(BlockErase_Timeout);
	FSMC_NOR_ReturnToReadMode();
	return status;
}

/*******************************************************************************
* Function Name  : FSMC_NOR_EraseChip
* Description    : Erases the entire chip.
* Input          : None
* Output         : None
* Return         : NOR_Status:The returned value can be: NOR_SUCCESS, NOR_ERROR
*                  or NOR_TIMEOUT
*******************************************************************************/
NOR_Status FSMC_NOR_EraseChip(void) {
	NOR_Status status;
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x2AAA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x2AAA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x5555), 0x0010);


	status = FSMC_NOR_GetStatus(ChipErase_Timeout);
	FSMC_NOR_ReturnToReadMode();
	return status;
}

/******************************************************************************
* Function Name  : FSMC_NOR_WriteHalfWord
* Description    : Writes a half-word to the NOR memory.
* Input          : - WriteAddr : NOR memory internal address to write to.
*                  - Data : Data to write.
* Output         : None
* Return         : NOR_Status:The returned value can be: NOR_SUCCESS, NOR_ERROR
*                  or NOR_TIMEOUT
*******************************************************************************/
NOR_Status FSMC_NOR_WriteHalfWord(long WriteAddr, short Data) {
	short t;
	NOR_Status status;

	NOR_WRITE(ADDR_SHIFT(0x05555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AAA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x05555), 0x00A0);
	NOR_WRITE((Bank1_NOR2_ADDR + WriteAddr), Data);
	for (t = 0; t < 100; t++);
	status = (FSMC_NOR_GetStatus(Program_Timeout));
	FSMC_NOR_ReturnToReadMode();
	return status;
}

/*******************************************************************************
* Function Name  : FSMC_NOR_WriteBuffer
* Description    : Writes a half-word buffer to the FSMC NOR memory.
* Input          : - pBuffer : pointer to buffer.
*                  - WriteAddr : NOR memory internal address from which the data
*                    will be written.
*                  - NumHalfwordToWrite : number of Half words to write.
* Output         : None
* Return         : NOR_Status:The returned value can be: NOR_SUCCESS, NOR_ERROR
*                  or NOR_TIMEOUT
*******************************************************************************/
NOR_Status FSMC_NOR_WriteBuffer(const short *pBuffer, long WriteAddr, long NumHalfwordToWrite) {
	NOR_Status status = NOR_ONGOING;

	do {
		/* Transfer data to the memory */
		status = FSMC_NOR_WriteHalfWord(WriteAddr, *pBuffer++);
		WriteAddr = WriteAddr + 2;
		NumHalfwordToWrite--;
	} while ((status == NOR_SUCCESS) && (NumHalfwordToWrite != 0));
	FSMC_NOR_ReturnToReadMode();

	return (status);
}

/*******************************************************************************
* Function Name  : FSMC_NOR_ProgramBuffer
* Description    : Writes a half-word buffer to the FSMC NOR memory. This function
*                  must be used only with S29GL128P NOR memory.
* Input          : - pBuffer : pointer to buffer.
*                  - WriteAddr: NOR memory internal address from which the data
*                    will be written.
*                  - NumHalfwordToWrite: number of Half words to write.
*                    The maximum allowed value is 32 Half words (64 bytes).
* Output         : None
* Return         : NOR_Status:The returned value can be: NOR_SUCCESS, NOR_ERROR
*                  or NOR_TIMEOUT
*******************************************************************************/
NOR_Status FSMC_NOR_ProgramBuffer(short *pBuffer, long WriteAddr, long NumHalfwordToWrite) {
	NOR_Status status;
	long lastloadedaddress = 0x00;
	long currentaddress = 0x00;
	long endaddress = 0x00;

	/* Initialize variables */
	currentaddress = WriteAddr;
	endaddress = WriteAddr + NumHalfwordToWrite - 1;
	lastloadedaddress = WriteAddr;

	/* Issue unlock command sequence */
	NOR_WRITE(ADDR_SHIFT(0x05555), 0x00AA);

	NOR_WRITE(ADDR_SHIFT(0x02AAA), 0x0055);

	/* Write Write Buffer Load Command */
	NOR_WRITE(ADDR_SHIFT(WriteAddr), 0x0025);
	NOR_WRITE(ADDR_SHIFT(WriteAddr), (NumHalfwordToWrite - 1));

	/* Load Data into NOR Buffer */
	while (currentaddress <= endaddress) {
		/* Store last loaded address & data value (for polling) */
		lastloadedaddress = currentaddress;

		NOR_WRITE(ADDR_SHIFT(currentaddress), *pBuffer++);
		currentaddress += 1;
	}

	NOR_WRITE(ADDR_SHIFT(lastloadedaddress), 0x29);

	status = FSMC_NOR_GetStatus(Program_Timeout);
	FSMC_NOR_ReturnToReadMode();
	return status;
}


/******************************************************************************
* Function Name  : FSMC_NOR_ReturnToReadMode
* Description    : Returns the NOR memory to Read mode.
* Input          : None
* Output         : None
* Return         : NOR_SUCCESS
*******************************************************************************/
NOR_Status FSMC_NOR_ReturnToReadMode(void) {
	NOR_WRITE(Bank1_NOR2_ADDR, 0x00F0);

	return (NOR_SUCCESS);
}

/******************************************************************************
* Function Name  : FSMC_NOR_Reset
* Description    : Returns the NOR memory to Read mode and resets the errors in
*                  the NOR memory Status Register.
* Input          : None
* Output         : None
* Return         : NOR_SUCCESS
*******************************************************************************/
NOR_Status FSMC_NOR_Reset(void) {
	NOR_WRITE(ADDR_SHIFT(0x05555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AAA), 0x0055);
	NOR_WRITE(Bank1_NOR2_ADDR, 0x00F0);
	FSMC_NOR_ReturnToReadMode();
	return (NOR_SUCCESS);
}

/******************************************************************************
* Function Name  : FSMC_NOR_GetStatus
* Description    : Returns the NOR operation status.
* Input          : - Timeout: NOR progamming Timeout
* Output         : None
* Return         : NOR_Status:The returned value can be: NOR_SUCCESS, NOR_ERROR
*                  or NOR_TIMEOUT
*******************************************************************************/
NOR_Status FSMC_NOR_GetStatus(long Timeout) {
	short val1 = 0x00, val2 = 0x00;
	NOR_Status status = NOR_ONGOING;
	//long timeout = Timeout;
	/*
	// Poll on NOR memory Ready/Busy signal ------------------------------------
	while((GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_6) != RESET) && (timeout > 0))
	{
	  timeout--;
	}

	timeout = Timeout;

	while((GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_6) == RESET) && (timeout > 0))
	{
	  timeout--;
	}
	*/
	// Get the NOR memory operation status -------------------------------------
	while ((Timeout != 0x00) && (status != NOR_SUCCESS)) {
		Timeout--;

		//Read DQ6 and DQ5
		//Read DQ6 and DQ2   SST39VF320

		val1 = *(unsigned short *)(Bank1_NOR2_ADDR);
		val2 = *(unsigned short *)(Bank1_NOR2_ADDR);

		// If DQ6 did not toggle between the two reads then return NOR_Success
		if ((val1 & 0x0040) == (val2 & 0x0040)) {
			return NOR_SUCCESS;
		}

		if((val1 & 0x0020) != 0x0020) {
		//if ((val1 & 0x0004) != 0x0004) {
			status = NOR_ONGOING;
		}

		val1 = *(unsigned short *)(Bank1_NOR2_ADDR);
		val2 = *(unsigned short *)(Bank1_NOR2_ADDR);

		if ((val1 & 0x0040) == (val2 & 0x0040)) {
			return NOR_SUCCESS;
		} else if ((val1 & 0x0020) == 0x0020) // ?????????????????????
			//else if((val1 & 0x0004) == 0x0004)
		{
			return NOR_ERROR;
		}
	}

	if (Timeout == 0x00) {
		status = NOR_TIMEOUT;
	}

	// Return the operation status
	return (status);
}

/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/
