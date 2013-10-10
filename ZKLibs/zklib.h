#ifndef __CNXIQP_H__
#define __CNXIQP_H__

/// \brief  CPU对其的边界.
#define ALIGNED_SIZE 4

/// \brief  计算一个对象的边界对齐后的大小.
/// 如果一个对象的实际大小为 n*ALIGNED_SIZE+1, n*ALIGNED_SIZE+2 ... (n+1)*ALIGNED_SIZE,
/// 该宏计算出来的大小为(n+1)*ALIGNED_SIZE.
#define ALIGNED_SIZEOF(a) (((sizeof(a) + ALIGNED_SIZE - 1) / ALIGNED_SIZE) * ALIGNED_SIZE)

/// \brief 计算数组成员的数量.
/// \note  array必须是数组.
#define ARRAY_MEMBER_NUMBER(array) (sizeof(array)/sizeof(array[0]))

#endif
