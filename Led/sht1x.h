#ifndef __SHT1X_H__
#define __SHT1X_H__

/// \brief   温湿度传感器初始化;
void SHT11Init(void);

/// \brief   读取温湿度;
/// \param   temp      输出指针, 存放读取到的温度, 单位0.1摄氏度;
/// \param   hmui      输出指针, 存放读取到的湿度, 单位0.1;
/// \return  !=0       成功;
/// \return  =0        失败;
int SHT11ReadTemperatureHumidity(int *temp, int *humi);

#endif
