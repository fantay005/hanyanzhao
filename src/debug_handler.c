#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rtc.h"
#include "second_datetime.h"
#include "led_lowlevel.h"
#include "softpwm_led.h"
#include "gsm.h"
#include "norflash.h"
#include "protocol.h"

void __sendAtCommandToGSM(const char *p) {
	extern int GsmTaskSendAtCommand(const char * atcmd);
	printf("SendAtCommandToGSM: ");
	if (GsmTaskSendAtCommand(p)) {
		printf("Succeed\n");
	} else {
		printf("Failed\n");
	}
}

static void __setGatewayParam(const char *p) {
	GMSParameter g;

	sscanf(p, "%*[^,]%*c%[^,]", g.GWAddr);
	sscanf(p, "%*[^,]%*c%*[^,]%*c%[^,]", g.serverIP);
	sscanf(p, "%*[^,]%*c%*[^,]%*c%*[^,]%*c%d", &(g.serverPORT));

	NorFlashWrite(NORFLASH_MANAGEM_ADDR, (const short *)&g, (sizeof(GMSParameter) + 1) / 2);
}

static void __QueryParamInfor(const char *p){
	
}

static void __erasureFlashChip(const char *p){
	NorFlashEraseChip();
}

static void __RALAYSWITCH(const char *p) {
	uint16_t Pin_array[] = {PIN_CTRL_1, PIN_CTRL_2, PIN_CTRL_3, PIN_CTRL_4, PIN_CTRL_5, PIN_CTRL_6, PIN_CTRL_7, PIN_CTRL_8};
	GPIO_TypeDef *Gpio_array[] ={GPIO_CTRL_1, GPIO_CTRL_2, GPIO_CTRL_3, GPIO_CTRL_4, GPIO_CTRL_5, GPIO_CTRL_6, GPIO_CTRL_7, GPIO_CTRL_8};
	char i;

	if(GPIO_ReadOutputDataBit(Gpio_array[0], Pin_array[0])){
		GPIO_SetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
		for(i = 0; i < 8; i++){
			GPIO_ResetBits(Gpio_array[i], Pin_array[i]);			
		}		
		GPIO_ResetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
	} else {
		GPIO_SetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
		for(i = 0; i < 8; i++){
			GPIO_SetBits(Gpio_array[i], Pin_array[i]);				
		}
		GPIO_ResetBits(GPIO_CTRL_EN, PIN_CRTL_EN);
	}
}
typedef struct {
	const char *prefix;
	void (*func)(const char *);
} DebugHandlerMap;

static const DebugHandlerMap __handlerMaps[] = {
	{ "AT", __sendAtCommandToGSM },
  { "WG", __setGatewayParam },
  { "ER", __erasureFlashChip},
	{ "E",  __QueryParamInfor},
	{ "ON", __RALAYSWITCH},
	{ NULL, NULL },
};

void DebugHandler(const char *msg) {
	const DebugHandlerMap *map;

	printf("DebugHandler: %s\n", msg);

	for (map = __handlerMaps; map->prefix != NULL; ++map) {
		if (0 == strncmp(map->prefix, msg, strlen(map->prefix))) {
			map->func(msg);
			return;
		}
	}
	printf("DebugHandler: Can not handler\n");
}
