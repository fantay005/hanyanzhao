#ifndef __SOUNDCONTROL_H__
#define __SOUNDCONTROL_H__

#include <stdbool.h>

typedef enum {
	SOUND_CONTROL_CHANNEL_GSM = 0x01,
	SOUND_CONTROL_CHANNEL_XFS = 0x02,
	SOUND_CONTROL_CHANNEL_MP3 = 0x04,
	SOUND_CONTROL_CHANNEL_FM = 0x08,
} SoundControlChannel;

void SoundControlInit(void);
void SoundControlSetChannel(SoundControlChannel channel, bool isOn);

#endif
