#ifndef __NORFLASH_H__
#define __NORFLASH_H__

#include <stdbool.h>
#include "fsmc_nor.h"

/********************************************************************************************************
* NOR_FLASH地址映射表
********************************************************************************************************/
//NOR_FLASH
#define NORFLASH_SECTOR_SIZE   				          ((u32)0x00001000)  
#define NORFLASH_MANAGEM_BASE  				          ((u32)0x00001000)/*网关参数-网关身份标识、经纬度、ZIGBEE频点、自动上传数据时间间隔*/
#define NORFLASH_BALLAST_NUM   				          ((u32)0x00002000)/*镇流器数目*/
#define NORFLASH_ONOFFTIME1   				          ((u32)0x00003000)/*开关灯时间1*/
#define NORFLASH_ONOFFTIME2   				          ((u32)0x00004000)/*开关灯时间2*/
#define NORFLASH_CHIP_ERASE                     ((u32)0x00005000)/*D*/ 
#define NORFLASH_BALLAST_BASE  				          ((u32)0x00006000)/*镇流器参数基址*/
#define NORFLASH_MANAGEM_ADDR                   ((u32)0x00100000)/*网关地址、服务器IP地址、端口号*/
#define NORFLASH_MANAGEM_TIMEOFFSET 		        ((u32)0x00101000)/*网关参数-开灯偏移、关灯偏移*/
#define NORFLASH_MANAGEM_WARNING   		          ((u32)0x00102000)/*网关参数-告警*/
#define NORFLASH_RESET_TIME           		    	((u32)0x00104000)/*重启时间*/
#define NORFLASH_RESET_COUNT                    ((u32)0x00105000)/*重启次数*/
#define NORFLASH_STRATEGY_BASE 				          ((u32)0x00106000)/**/
#define NORFLASH_STRATEGY_OK_OFFSET 		       	((u32)0x00000d00)/*策略完整标识*/
#define NORFLASH_PARAM_OFFSET   				        ((u32)0x00001000)
#define NORFLASH_STRATEGY_OFFSET        		    ((u32)0x00001000)

/*镇流器参数及策略扇区内偏移*/
#define PARAM_ZIGBEE_ADDR_OFFSET                ((u32)0x00000000) /*共4*2个字节ASCII码，存储ZIGBEE地址*/
#define PARAM_TIME_FALG_OFFSET                  ((u32)0x00000008) /*共12*2个字节ASCII码，存储参数同步标识*/
#define PARAM_RATED_POWER_OFFSET                ((u32)0x00000020) /*共4*2个字节ASCII码，存储标称功率值*/
#define PARAM_LOOP_NUM_OFFSET                   ((u32)0x00000028) /*共1*2个字节ASCII码，存储所属回路*/
#define PARAM_LAMP_POST_NUM_OFFSET              ((u32)0x0000002A) /*共4*2个字节ASCII码，存储所属灯杆号*/
#define PARAM_LAMP_TYPE_OFFSET                  ((u32)0x00000032) /*共1*2个字节ASCII码，存储光源类型*/
#define PARAM_PHASE_OFFSET                      ((u32)0x00000034) /*共1*2个字节ASCII码，存储负载相线*/
#define PARAM_PORPERTY_OFFSET                   ((u32)0x00000036) /*共2*2个字节ASCII码，存储主辅投属性*/
#define STRATEGY_ZIGBEE_ADDR_OFFSET             ((u32)0x00000000) /*共4*2个字节ASCII码，存储ZIGBEE地址*/
#define STRATEGY_TIME_FALG_OFFSET               ((u32)0x00000008) /*共12*2个字节ASCII码，存储策略同步标识*/
#define STRATEGY_TYPE_OFFSET                    ((u32)0x00000020) /*共2*2个字节ASCII码，存储方案类型*/
#define STRATEGY_STAGE_NUM_OFFSET               ((u32)0x00000024) /*共1*2个字节ASCII码，存储调光段数*/
#define STRATEGY_FIRST_STATE_OFFSET             ((u32)0x00000026) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_SECOND_STATE_OFFSET            ((u32)0x00000032) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_THIRD_STATE_OFFSET             ((u32)0x0000003E) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_FOURTH_STATE_OFFSET            ((u32)0x0000004A) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_FIFTH_STATE_OFFSET             ((u32)0x00000056) /*共6*2个字节ASCII码，存储调光功率及时间*/

/*日出日落时间扇区内偏移*/
#define NORFLASH_RISE_TIME_OFFSET				        0
#define NORFLASH_SET_TIME_OFFSET				        1

/*网管地址、服务器IP、端口号扇区内偏移（共64字节）*/
#define NORFLASH_MANNGEM_ADDR_OFFSET			      ((u32)0x00000000)//(0x00000000-0x00000012)?10*2???ASCII???????
#define NORFLASH_DNS1_NUM_OFFSET			        	((u32)0x00000014)//(0x00000014-0x00000014)?1*2?????DNS1???
#define NORFLASH_DNS1_OFFSET			    	        ((u32)0x00000016)//(0x00000016-0x00000032)?15*2?????DNS1
#define NORFLASH_DNS2_NUM_OFFSET			         	((u32)0x00000034)//(0x00000034-0x00000034)?1*2?????DNS2???
#define NORFLASH_DNS2_OFFSET			            	((u32)0x00000036)//(0x00000036-0x00000052)?15*2?????DNS2
#define NORFLASH_IP_NUM_OFFSET				          ((u32)0x00000054)//(0x00000054-0x00000054)?1*2?????IP???
#define NORFLASH_IP_OFFSET			    	          ((u32)0x00000056)//(0x00000056-0x00000072)?15*2?????IP
#define NORFLASH_PORT_NUM_OFFSET				        ((u32)0x00000074)//(0x00000074-0x00000074)?1*2?????DNS2???
#define NORFLASH_PORT_OFFSET			            	((u32)0x00000076)//(0x00000076-0x0000007E)?15*2?????DNS2 
/********************************************************************************************************
* SRAM?????
********************************************************************************************************/     
//SRAM
#define SRAM_SIZE              				          ((u32)0x00080000)  
#define SRAM_BALLAST_BASE      				          ((u32)0x00000000)
#define SRAM_BALLAST_OFFSET    				          ((u32)0x00000400)

#define DATA_MEM_OFFSET                          50

#define SRAM_PROPER_BASE    		            ((u32)0x00050000)
#define SRAM_LOOP_OFFSET						((u32)0x00004000)
#define SRAM_ADDITIONAL_OFFSET        			((u32)0x00000400)
#define SRAM_PROPER_TG_BASE    		            ((u32)0x00070000)
#define SRAM_LOOP_TG_OFFSET						((u32)0x00000400)
//??????????
#define SRAM_ZIGBEE_ADDR_OFFSET					((u32)0x00000000)//(0x00000000-0x00000000)?1*2?????ZIGBEE??
#define SRAM_PARAM_FLAG_OFFSET					((u32)0x00000002)//(0x00000002-0x00000018)?12*2???ASCII?????????
#define SRAM_STRATEGY_FLAG_OFFSET				((u32)0x0000001A)//(0x0000001A-0x00000030)?12*2???ASCII?????????
#define SRAM_DIMMING_PERCENT_OFFSET				((u32)0x00000032)//(0x00000032-0x00000034)?2*2???ASCII????????
#define SRAM_BALLAST_STATUS_OFFSET				((u32)0x00000036)//(0x00000036-0x00000038)?2*2???ASCII???????
#define SRAM_VOLTAGE_OFFSET				        ((u32)0x0000003A)//(0x0000003A-0x00000040)?4*2???ASCII???????
#define SRAM_CURRENT_OFFSET				        ((u32)0x00000042)//(0x00000042-0x00000048)?4*2???ASCII???????
#define SRAM_POWER_OFFSET				        ((u32)0x0000004A)//(0x0000004A-0x00000050)?4*2???ASCII???????
#define SRAM_LAMP_VOLTAGE_OFFSET				((u32)0x00000052)//(0x00000052-0x00000058)?4*2???ASCII???????
#define SRAM_PFC_VLATAGE_OFFSET				    ((u32)0x0000005A)//(0x0000005A-0x00000060)?4*2???ASCII???PFC??
#define SRAM_TEMPERATURE_OFFSET			   	    ((u32)0x00000062)//(0x00000062-0x00000068)?4*2???ASCII?????
#define SRAM_RUNNING_TIME_OFFSET				((u32)0x0000006A)//(0x0000006A-0x00000074)?6*2???ASCII?????????
/*******************************************************************************************************/

void NorFlashInit(void);
void NorFlashWrite(uint32_t flash, const short *ram, int len);
void NorFlashEraseParam(uint32_t flash);
void NorFlashRead(uint32_t flash, short *ram, int len);

bool NorFlashMutexLock(uint32_t time);
void NorFlashMutexUnlock(void);


#endif
