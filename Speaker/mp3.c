#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h" 
#include "queue.h" 
#include "semphr.h"
#include "task.h"
#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_exti.h"
#include "soundcontrol.h"
#include "mp3.h"

#define MP3_TASK_STACK_SIZE	  (configMINIMAL_STACK_SIZE + 256)

/*
 * 1-XCS   VS1003片选
 * 2-RST   VS1003复位
 * 3-XDCS  VS1003数据命令选择
 * 4-DREQ  VS1003数据中断
*/

#define TCS   (1<<4)  // PA4-XCS													    
#define TCS_SET(x)  GPIOA->ODR=(GPIOA->ODR&~TCS)|(x ? TCS:0)

#define RST   (1<<5)  // PC5-RST   
#define TRST_SET(x)  GPIOC->ODR=(GPIOC->ODR&~RST)|(x ? RST:0)

#define XDCS   (1<<8)  // PB8-XDCS  
#define TXDCS_SET(x)  GPIOB->ODR=(GPIOB->ODR&~XDCS)|(x ? XDCS:0)

#define XDCS   (1<<8)  // PB8-XDCS  
#define TXDCS_SET(x)  GPIOB->ODR=(GPIOB->ODR&~XDCS)|(x ? XDCS:0)

#define DREQ  GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)

/* VS1003(音频解码芯片) 命令宏定义 */
#define VS_WRITE_COMMAND 			0x02
#define VS_READ_COMMAND 			0x03		 		 

/* VS1003(音频解码芯片) 功能寄存器 */
#define SPI_MODE        			0x00   
#define SPI_STATUS      			0x01   
#define SPI_BASS        			0x02   
#define SPI_CLOCKF      			0x03   
#define SPI_DECODE_TIME 			0x04   
#define SPI_AUDATA      			0x05   
#define SPI_WRAM        			0x06   
#define SPI_WRAMADDR    			0x07   
#define SPI_HDAT0       			0x08   
#define SPI_HDAT1       			0x09   
#define SPI_AIADDR      			0x0a   
#define SPI_VOL         			0x0b   
#define SPI_AICTRL0     			0x0c   
#define SPI_AICTRL1     			0x0d   
#define SPI_AICTRL2     			0x0e   
#define SPI_AICTRL3     			0x0f

/* VS1003(音频解码芯片) 模式寄存器16位 */   
#define SM_DIFF         			0x01   
#define SM_JUMP         			0x02   
#define SM_RESET        			0x04   
#define SM_OUTOFWAV     			0x08   
#define SM_PDOWN        			0x10   
#define SM_TESTS        			0x20   
#define SM_STREAM       			0x40   
#define SM_PLUSV        			0x80   
#define SM_DACT         			0x100   
#define SM_SDIORD       			0x200   
#define SM_SDISHARE     			0x400   
#define SM_SDINEW       			0x800   
#define SM_ADPCM        			0x1000   
#define SM_ADPCM_HP     			0x2000 


#define MP3CMD_InitVS1003			0x11
#define MP3CMD_Play				  	0x12
#define MP3CMD_Pause			  	0x13
#define MP3CMD_Stop				  	0x14
#define MP3CMD_Next				  	0x15
#define MP3CMD_TestVS1003			0x16

#define SPI1_SPEED_2    0
#define SPI1_SPEED_4    1
#define SPI1_SPEED_8    2
#define SPI1_SPEED_16   3
#define SPI1_SPEED_32   4
#define SPI1_SPEED_64   5
#define SPI1_SPEED_128  6
#define SPI1_SPEED_256  7 //281K

u8 VS_VOLT=255;	 //默认音量200
u8 VS_BASS=0; 	//默认非超重低音

static void VS1003_SPI_Init() {
	SPI_InitTypeDef SPI_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;
	 NVIC_InitTypeDef NVIC_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	            //CS

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);				//SCK,MOSI

//	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 ;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
//	GPIO_Init(GPIOA, &GPIO_InitStructure);				 //MISO

	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //双工模式
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master; //SPI主模式
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b; //8bit数据

	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High; //CLK空闲时为低电平
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge; //CLK上升沿采样，因为上升沿是第一个边沿动作，所以也可以理解为第一个边沿采样
	
//	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low; //CLK空闲时为低电平
//	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge; //CLK上升沿采样，因为上升沿是第一个边沿动作，所以也可以理解为第一个边沿采样

	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; //片选用软件控制
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256; //SPI频率
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB; //高位在前
	SPI_InitStructure.SPI_CRCPolynomial = 7; //crc7，stm32spi带硬件ecc
	SPI_Init(SPI1, &SPI_InitStructure);
	SPI_Cmd(SPI1, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);		  //MP3_IRQ

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);		   //MP3_SYNC

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOC, &GPIO_InitStructure);		   //MP3_RST

	EXTI_InitStructure.EXTI_Line = EXTI_Line1; //选择中断线路2 3 5
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt; //设置为中断请求，非事件请求
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; //设置中断触发方式为上下降沿触发
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;                                          //外部中断使能
    EXTI_Init(&EXTI_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource1);	  

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	
	NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;     //选择中断通道1
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; //抢占式中断优先级设置为0
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;        //响应式中断优先级设置为0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                                   //使能中断
    NVIC_Init(&NVIC_InitStructure);
}

//SPI1口读写一个字节
//TxData:要发送的字节		 
//返回值:读取到的字节
uint8_t inline SPI1_ReadWriteByte(uint8_t TxData)
{
	while((SPI1->SR&1<<1)==0);//等待发送区空				  
	SPI1->DR=TxData;	 	  //发送一个byte   
	while((SPI1->SR&1<<0)==0);//等待接收完一个byte  
	return SPI1->DR;          //返回收到的数据				    
}

void inline VS1003_WriteData( uint8_t Data)
{		  
   SPI1_ReadWriteByte( Data );
} 

void SPI1_SetSpeed(uint8_t SpeedSet)
{
//  SPI1->CR1&=0<<6;
	SPI1->CR1&=0XFFC7;//Fsck=Fcpu/256
	if(SpeedSet==SPI1_SPEED_2)//二分频
	{
		SPI1->CR1|=0<<3;//Fsck=Fpclk/2=36Mhz	
	}else if(SpeedSet==SPI1_SPEED_4)//十六分频
	{
		SPI1->CR1|=1<<3;//Fsck=Fpclk/16=4.5Mhz
	}else if(SpeedSet==SPI1_SPEED_8)//八分频 
	{
		SPI1->CR1|=2<<3;//Fsck=Fpclk/8=9Mhz	
	}else if(SpeedSet==SPI1_SPEED_16)//十六分频
	{
		SPI1->CR1|=3<<3;//Fsck=Fpclk/16=4.5Mhz
	}else if(SpeedSet==SPI1_SPEED_32)//十六分频
	{
		SPI1->CR1|=4<<3;//Fsck=Fpclk/16=4.5Mhz
	}else if(SpeedSet==SPI1_SPEED_64)//十六分频
	{
		SPI1->CR1|=5<<3;//Fsck=Fpclk/16=4.5Mhz
	}else if(SpeedSet==SPI1_SPEED_128)//十六分频
	{
		SPI1->CR1|=6<<3;//Fsck=Fpclk/16=4.5Mhz
	}else			 	 //256分频
	{
		SPI1->CR1|=7<<3; //Fsck=Fpclk/256=281.25Khz 低速模式
	}
	SPI1->CR1|=1<<6; //SPI设备使能	  
}

void Mp3WriteRegister(uint8_t address,uint16_t data)
{  
    while(DREQ==0);//等待空闲
	SPI1_SetSpeed(SPI1_SPEED_256);//低速 
	 
	TXDCS_SET(1); //MP3_DATA_CS=1;
	TCS_SET(0); //MP3_CMD_CS=0; 
	
	SPI1_ReadWriteByte(VS_WRITE_COMMAND);//发送VS1003的写命令
	SPI1_ReadWriteByte(address); //地址
	SPI1_ReadWriteByte(data>>8); //发送高八位
	SPI1_ReadWriteByte(data);	 //第八位
	TCS_SET(1);         //MP3_CMD_CS=1; 
	SPI1_SetSpeed(SPI1_SPEED_8);//高速
} 

uint16_t Mp3ReadRegister(uint8_t reg)
{
   uint16_t value;
    
   while(  DREQ ==0 );           /* 等待空闲 */
   SPI1_SetSpeed( SPI1_SPEED_256 );
   TXDCS_SET(1);     
   TCS_SET(0);     
   SPI1_ReadWriteByte(VS_READ_COMMAND);/* 发送VS1003的读命令 */
   SPI1_ReadWriteByte( reg );   
   value = SPI1_ReadWriteByte(0xff);		
   value = value << 8;
   value |= SPI1_ReadWriteByte(0xff); 
   TCS_SET(1);   
   SPI1_SetSpeed( SPI1_SPEED_16);
   return value;                                  
}

void VS_SET_VOL(uint8_t volt,uint8_t bass)
{
	u16 temp_volt,temp_bass;
	volt=255-volt;
	temp_volt=volt;
   	temp_volt=(temp_volt<<8)+volt;
	temp_bass=(temp_bass<<8)+bass;
    Mp3WriteRegister(SPI_VOL,temp_volt); //设音量 
	Mp3WriteRegister(SPI_BASS,temp_bass);//BASS 
}

//重设解码时间                          
void ResetDecodeTime(void)
{
	Mp3WriteRegister(SPI_DECODE_TIME,0x0000);
	Mp3WriteRegister(SPI_DECODE_TIME,0x0000);//操作两次
}

void Vs1003SoftReset(void)
{	 
	uint8_t retry; 				   
	while((DREQ)==0);//等待软件复位结束
	SPI1_ReadWriteByte(0X00);//启动传输
	retry=0;
	while(Mp3ReadRegister(SPI_MODE)!=0x0C04)// 软件复位,新模式  
	{
		Mp3WriteRegister(SPI_MODE,0x0C04);// 软件复位,新模式
		vTaskDelay(configTICK_RATE_HZ / 500);//等待至少1.35ms 
		if(retry++>100)break; 
	}	 				  
	while (DREQ == 0);//等待软件复位结束	   

	retry=0;
	while(Mp3ReadRegister(SPI_CLOCKF)!=0X9800)//设置vs1003的时钟,3倍频 ,1.5xADD 
	{
		Mp3WriteRegister(SPI_CLOCKF,0X9800);//设置vs1003的时钟,3倍频 ,1.5xADD
		vTaskDelay(configTICK_RATE_HZ / 500);//等待至少1.35ms 
		if(retry++>100)break; 
	}		   
	retry=0;
	while(Mp3ReadRegister(SPI_AUDATA)!=0X1F41) //设置采样率
	{
		Mp3WriteRegister(SPI_AUDATA,0X1F41);
		vTaskDelay(configTICK_RATE_HZ / 500);//等待至少1.35ms 
		if(retry++>100)break; 
	}
	//Vs1003_CMD_Write(SPI_CLOCKF,0X9800); 	    
	//Vs1003_CMD_Write(SPI_AUDATA,0XBB81); //采样率48k，立体声	
	VS_SET_VOL(VS_VOLT,VS_BASS);		 
	ResetDecodeTime();//复位解码时间	    
    //向vs1003发送4个字节无效数据，用以启动SPI发送
    TXDCS_SET( 0 );//选中数据传输
	SPI1_ReadWriteByte(0XFF);
	SPI1_ReadWriteByte(0XFF);
	SPI1_ReadWriteByte(0XFF);
	SPI1_ReadWriteByte(0XFF);
	TXDCS_SET( 1 );//取消数据传输
	vTaskDelay(configTICK_RATE_HZ / 500);
} 

void Mp3Reset(void)
{
	TRST_SET( 0 );
	vTaskDelay(configTICK_RATE_HZ / 50 );
	TXDCS_SET( 1 );	//取消数据传输
	TCS_SET( 1 );//取消数据传输
	vTaskDelay(configTICK_RATE_HZ / 50 );
	TRST_SET( 1 );    
	while(DREQ==0);	//等待DREQ为高
	vTaskDelay(configTICK_RATE_HZ / 50 );				 
}



void VsSineTest(void)
{											    
	Mp3Reset();	 
	Mp3WriteRegister(0x0b,0X2020);	  //设置音量	 
 	Mp3WriteRegister(SPI_MODE,0x0820);//进入vs1003的测试模式	    
	while (DREQ == 0);     //等待DREQ为高
 	//向vs1003发送正弦测试命令：0x53 0xef 0x6e n 0x00 0x00 0x00 0x00
    TXDCS_SET( 0 );//选中数据传输
	SPI1_ReadWriteByte(0x53);
	//其中n = 0x24, 设定vs1003所产生的正弦波的频率值，具体计算方法见vs1003的datasheet
   	SPI1_ReadWriteByte(0xef);
	SPI1_ReadWriteByte(0x6e);
	SPI1_ReadWriteByte(0x24);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	vTaskDelay(configTICK_RATE_HZ / 5);
	TXDCS_SET( 1 );
    //退出正弦测试
    TXDCS_SET( 0 );//选中数据传输
	SPI1_ReadWriteByte(0x45);
	SPI1_ReadWriteByte(0x78);
	SPI1_ReadWriteByte(0x69);
	SPI1_ReadWriteByte(0x74);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	vTaskDelay(configTICK_RATE_HZ / 5);
	TXDCS_SET( 1 );		 

    //再次进入正弦测试并设置n值为0x44，即将正弦波的频率设置为另外的值
    TXDCS_SET( 0 );//选中数据传输      
	SPI1_ReadWriteByte(0x53);
	SPI1_ReadWriteByte(0xef);
	SPI1_ReadWriteByte(0x6e);
	SPI1_ReadWriteByte(0x44);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	vTaskDelay(configTICK_RATE_HZ / 5);
	TXDCS_SET( 1 );	
    //退出正弦测试
    TXDCS_SET( 0 );//选中数据传输
	SPI1_ReadWriteByte(0x45);
	SPI1_ReadWriteByte(0x78);
	SPI1_ReadWriteByte(0x69);
	SPI1_ReadWriteByte(0x74);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	vTaskDelay(configTICK_RATE_HZ / 5);
	TXDCS_SET( 1 );
}

typedef struct {
	uint8_t cmd;
	void *dat;
} VS1003TaskMessage;


typedef struct {
	unsigned char *dat;
	int len;
} MusicData;

static xQueueHandle __VS1003queue;
static xSemaphoreHandle __semaphore = NULL;

#define VS1003_OPEN_IDLE 0
#define VS1003_DATA 1


void Vs1003Idle(void) {
	VS1003TaskMessage msg;
	msg.cmd = VS1003_OPEN_IDLE;
	xQueueSend(__VS1003queue, &msg, configTICK_RATE_HZ);
}


void EXTI1_IRQHandler(void)
{
	portBASE_TYPE n;
	EXTI_ClearITPendingBit(EXTI_Line1);
	if (pdTRUE == xSemaphoreGiveFromISR(__semaphore, &n)) {
		if (n) {
			taskYIELD();
		}
	}			
}

//void Mp3Play(const char *music, int len)
//{						  
//	VS1003TaskMessage msg;
//	DataNode *dat = (DataNode *)pvPortMalloc(len + sizeof(DataNode));
//	dat->dat = (unsigned char *)&dat[1];
//	memcpy(dat->dat, music, len);
//	dat->len = len;
//	msg.cmd = VS1003_DATA;
//	msg.dat = dat;	
//	xQueueSend(__VS1003queue, &msg, configTICK_RATE_HZ*5);
//}

void VS1003_Play(const unsigned char *data, int len) {
	MusicData music;
	music.dat = pvPortMalloc(len);
	memcpy(music.dat, data, len);
	music.len = len;
	xQueueSend(__VS1003queue, &music, configTICK_RATE_HZ*5);	  
}

static void VS1003_WriteDataSafe(unsigned char dat) {

	while (DREQ == 0) {	
		xSemaphoreTake(__semaphore, configTICK_RATE_HZ);
	}
//	TXDCS_SET( 0 );
	VS1003_WriteData(dat);
//	TXDCS_SET( 1 );
}


void vMP3(void *parameter) {
	int rc;
	MusicData msg;
	__semaphore = xQueueGenericCreate(1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE );
	__VS1003queue = xQueueCreate(5, sizeof(MusicData));
	Mp3Reset();
	Vs1003SoftReset();
	SoundControlSetChannel(SOUND_CONTROL_CHANNEL_MP3, 1); 
	printf("MP3: loop again\n");

	while(1) {
		rc = xQueueReceive(__VS1003queue, &msg, configTICK_RATE_HZ * 5);
		if (rc == pdTRUE) {
			int i;
			for (i = 0; i < msg.len; ++i) {
				VS1003_WriteDataSafe(msg.dat[i]);
			}
			vPortFree(msg.dat);			
		}		
	}

//	while(1) {
//			rc = xQueueReceive(__VS1003queue, &msg, configTICK_RATE_HZ * 5);
//			if (rc != pdTRUE) 
//			    continue;
//
//			if (msg.cmd == VS1003_DATA) {
//				__appendDataNode(msg.dat);			
//			}
//
//			if (msg.cmd == VS1003_OPEN_IDLE) {
//				if (first) {
//					const unsigned char *dat = &(first->dat[first->index]);
//					unsigned short i;
//					for (i = 0; i < 32; ++i) {
//						if (i + first->index >= first->len) {
//							__removeFirstNode();
//							break;
//						}
//						VS1003_WriteData(*dat++);						
//					}
//				}	
//			}

//			vTaskDelay(configTICK_RATE_HZ / 500);
//			if (msg.cmd = VS1003_OPEN_IDLE) {
////		       if(DREQ !=0 ){    /* 等待空闲 */
//			   TXDCS_SET(0);
//				or(dat = 0; dat < 32; dat++){
//		                VS1003_WriteData(music[count++]);
//					 } 
//					 TXDCS_SET( 1 );
//			   }
//
//			   if (count >= sizeof(music)){
//					vTaskDelay(5 * configTICK_RATE_HZ );
//			   		TXDCS_SET(1);
//					count = 0;
//					vTaskDelay(5 * configTICK_RATE_HZ );
//					Mp3Reset();
//                	Vs1003SoftReset();
//			   }


//		if ( DREQ != 0 )	      			/* 等待DREQ为高，请求数据输入 */
//			{
//             	Delay_us(10);
//			    TCS_SET(1);
//				for (dat=0; dat<32; dat++ ) /* VS1003的FIFO只有32个字节的缓冲 */
//				{										
//					VS1003_WriteData((uint8_t*)music[count]);										
//					count++;
//				}
//			}
//	}
}


void MP3Init(void) {
	VS1003_SPI_Init();
	xTaskCreate(vMP3, (signed portCHAR *) "MP3", MP3_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 8, NULL);
}

