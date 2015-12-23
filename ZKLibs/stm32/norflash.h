#ifndef __NORFLASH_H__
#define __NORFLASH_H__

#include <stdbool.h>
#include <stdint.h>
#include "fsmc_nor.h"

/********************************************************************************************************
* NOR_FLASH地址映射表
********************************************************************************************************/
//NOR_FLASH
#define NORFLASH_SECTOR_SIZE   				          ((uint32_t)0x00001000)/*扇区大小*/ 
#define NORFLASH_MANAGEM_BASE  				          ((uint32_t)0x00001000)/*网关参数-网关身份标识、经纬度、ZIGBEE频点、自动上传数据时间间隔*/
#define NORFLASH_BALLAST_NUM   				          ((uint32_t)0x00002000)/*镇流器数目*/
#define NORFLASH_ONOFFTIME1   				          ((uint32_t)0x00003000)/*开关灯时间1*/
#define NORFLASH_ONOFFTIME2   				          ((uint32_t)0x00004000)/*开关灯时间2*/
#define NORFLASH_CHIP_ERASE                     ((uint32_t)0x00005000)/*D*/ 

#define NORFLASH_END_LIGHT_ADDR                 ((uint32_t)0x00016000)/*所有末端灯ZIGBEE地址存放处*/
#define NORFLASH_LIGHT_NUMBER                   ((uint32_t)0x00017000)/*下载的灯参数量，即控制的灯总量*/
#define NORFLASH_BALLAST_BASE  				          ((uint32_t)0x00018000)/*Zigbee1 镇流器参数基址*/
#define NORFLASH_BSN_PARAM_BASE                 ((uint32_t)0x00218000)/*Zigbee2 镇流器参数基址*/
#define NORFLASH_MANAGEM_ADDR                   ((uint32_t)0x00400000)/*网关地址、服务器IP地址、端口号*/
#define NORFLASH_MANAGEM_TIMEOFFSET 		        ((uint32_t)0x00401000)/*网关参数-开灯偏移、关灯偏移*/
#define NORFLASH_MANAGEM_WARNING   		          ((uint32_t)0x00402000)/*网关参数-告警*/
#define NORFLASH_RESET_TIME           		    	((uint32_t)0x00404000)/*重启时间*/
#define NORFLASH_RESET_COUNT                    ((uint32_t)0x00405000)/*重启次数*/
#define NORFLASH_ELEC_UPDATA_TIME               ((uint32_t)0x00406000)/*电量上传时间*/
#define NORFLASH_STRATEGY_BASE 				          ((uint32_t)0x00418000)/*Zigbee1 策略参数基址*/
#define NORFLASH_STY_PARAM_BASE                 ((uint32_t)0x00618000)/*Zigbee2 策略参数基址*/
#define NORFLASH_STRATEGY_OK_OFFSET 		       	((uint32_t)0x00000d00)/*策略完整标识*/
#define NORFLASH_PARAM_OFFSET   				        ((uint32_t)0x00001000)
#define NORFLASH_STRATEGY_OFFSET        		    ((uint32_t)0x00001000)

/*镇流器参数及策略扇区内偏移*/
#define PARAM_ZIGBEE_ADDR_OFFSET                ((uint32_t)0x00000000) /*共4*2个字节ASCII码，存储ZIGBEE地址*/
#define PARAM_TIME_FALG_OFFSET                  ((uint32_t)0x00000008) /*共12*2个字节ASCII码，存储参数同步标识*/
#define PARAM_RATED_POWER_OFFSET                ((uint32_t)0x00000020) /*共4*2个字节ASCII码，存储标称功率值*/
#define PARAM_LOOP_NUM_OFFSET                   ((uint32_t)0x00000028) /*共1*2个字节ASCII码，存储所属回路*/
#define PARAM_LAMP_POST_NUM_OFFSET              ((uint32_t)0x0000002A) /*共4*2个字节ASCII码，存储所属灯杆号*/
#define PARAM_LAMP_TYPE_OFFSET                  ((uint32_t)0x00000032) /*共1*2个字节ASCII码，存储光源类型*/
#define PARAM_PHASE_OFFSET                      ((uint32_t)0x00000034) /*共1*2个字节ASCII码，存储负载相线*/
#define PARAM_PORPERTY_OFFSET                   ((uint32_t)0x00000036) /*共2*2个字节ASCII码，存储主辅投属性*/
#define STRATEGY_ZIGBEE_ADDR_OFFSET             ((uint32_t)0x00000000) /*共4*2个字节ASCII码，存储ZIGBEE地址*/
#define STRATEGY_TIME_FALG_OFFSET               ((uint32_t)0x00000008) /*共12*2个字节ASCII码，存储策略同步标识*/
#define STRATEGY_TYPE_OFFSET                    ((uint32_t)0x00000020) /*共2*2个字节ASCII码，存储方案类型*/
#define STRATEGY_STAGE_NUM_OFFSET               ((uint32_t)0x00000024) /*共1*2个字节ASCII码，存储调光段数*/
#define STRATEGY_FIRST_STATE_OFFSET             ((uint32_t)0x00000026) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_SECOND_STATE_OFFSET            ((uint32_t)0x00000032) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_THIRD_STATE_OFFSET             ((uint32_t)0x0000003E) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_FOURTH_STATE_OFFSET            ((uint32_t)0x0000004A) /*共6*2个字节ASCII码，存储调光功率及时间*/
#define STRATEGY_FIFTH_STATE_OFFSET             ((uint32_t)0x00000056) /*共6*2个字节ASCII码，存储调光功率及时间*/


#define UPDATA_FLAG_STORE_SECTOR                ((uint32_t)0x0800F800) /*是否需要升级结构体保存在内部FLASH中*/


#define UNICODE_TABLE_ADDR (0x0E0000)
#define UNICODE_TABLE_END_ADDR (UNICODE_TABLE_ADDR + 0x3B2E)
#define GBK_TABLE_OFFSET_FROM_UNICODE (0x3B30)
#define GBK_TABLE_ADDR (UNICODE_TABLE_ADDR + GBK_TABLE_OFFSET_FROM_UNICODE)
#define GBK_TABLE_END_ADDR (UNICODE_TABLE_END_ADDR + GBK_TABLE_OFFSET_FROM_UNICODE)

void NorFlashInit(void);
void NorFlashWrite(uint32_t flash, const short *ram, int len);
void NorFlashEraseParam(uint32_t flash);
void NorFlashRead(uint32_t flash, short *ram, int len);
void NorFlashEraseChip(void);

bool NorFlashMutexLock(uint32_t time);
void NorFlashMutexUnlock(void);


#endif
