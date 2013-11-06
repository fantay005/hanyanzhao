#ifndef __SOUNDCONTROL_H__
#define __SOUNDCONTROL_H__

#include <stdbool.h>
#include <stdint.h>

/// 声音控制通道.
typedef enum {
	/// GSM通道.
	SOUND_CONTROL_CHANNEL_GSM = 0x01,
	/// 讯飞语音模块通道.
	SOUND_CONTROL_CHANNEL_XFS = 0x40,
	/// MP3播放通道.
	SOUND_CONTROL_CHANNEL_MP3 = 0x02,
	/// FM收音通道.
	SOUND_CONTROL_CHANNEL_FM = 0x04,
} SoundControlChannel;

/// 初始化声音控制.
void SoundControlInit(void);
/// 声音控制.
/// \param channels    需要控制的通道, 可以是SoundControlChannel的任意或组合.
/// \param isOn        !=false 打开相应的通道.
/// \param isOn        =false  关闭相应的通道.
void SoundControlSetChannel(uint32_t channels, bool isOn);

#endif
