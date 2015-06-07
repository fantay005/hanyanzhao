#ifndef __ATCMD_H__
#define __ATCMD_H__

#include <stdbool.h>
#include "FreeRTOS.h"

/// \brief  如果ATCommand接受任何回应, prefix需要传入则个参数.
#define ATCMD_ANY_REPLY_PREFIX  ((const char *)0xFFFFFFFF)

/// \brief  初始化.
/// 初始化ATCommand相关函数运行需要的环境.
void ATCommandRuntimeInit(void);

/// \brief  发送AT命令并等待返回.
/// \param  cmd[in]     需要发送的AT命令, 需要包括'\r'字符.
/// \param  prefix[in]  需要等待的回应的开始字符串.
/// \param  timoutTick  等待回应最长的时间.
/// \return !=NULL      发送命令正确, 并且已经等到回应.
/// \return ==NULL      发送命令错误, 或等待回应超时.
/// \note               当返回值不为空时, 使用完返回值之后需要调用AtCommandDropReplyLine释放内存.
char *ATCommand(const char *cmd, const char *prefix, int timeoutTick);

/// \brief  释放AT命令返回数据.
/// \param  line[in]   需要释放的AT命令返回数据.
void AtCommandDropReplyLine(char *line);

/// \brief  发送AT命令并检查回应.
/// \param  cmd[in]     需要发送的AT命令, 需要包括'\r'字符.
/// \param  prefix[in]  需要等待的回应的开始字符串.
/// \param  timoutTick  等待回应最长的时间.
/// \return true        在等待时间内收到正确回应.
/// \return false       在等待时间内未收到正确回应.
bool ATCommandAndCheckReply(const char *cmd, const char *prefix, int timeoutTick);


/// \brief  发送AT命令并检查回应, 直到收到正确回应.
/// \param  cmd[in]     需要发送的AT命令, 需要包括'\r'字符.
/// \param  prefix[in]  需要等待的回应的开始字符串.
/// \param  timoutTick  每次等待回应最长的时间.
/// \param  times       最多等待的次数.
/// \return true        在等待时间内收到正确回应.
/// \return false       在等待时间内未收到正确回应.
bool ATCommandAndCheckReplyUntilOK(const char *cmd, const char *prefix, int timeoutTick, int times);

/// \brief 该函数是 串口接收到以回车结束的数据之后需要调用的.
/// 该函数是吧串口接收到的数据放入AT命令处理的队列中.
/// \param  line[in]    收到的数据.
/// \param  len[in]     收到的数据的长度.
/// \param  pxHigherPriorityTaskWoken[out]  存放是否需要任务调度的标志.
/// \return true        数据成功放入AT命令处理队列.
/// \return false       数据放入AT命令处理队列失败.
bool ATCommandGotLineFromIsr(const char *line, unsigned char len, portBASE_TYPE *pxHigherPriorityTaskWoken);

#endif
