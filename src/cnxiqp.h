#ifndef __CNXIQP_H__
#define __CNXIQP_H__

#define ALIGNED_SIZE 4

// 0, 1, 2 .. ALIGNED_SIZE-1 ==> ALIGNED_SIZE
// ALIGNED_SIZE, ALIGNED_SIZE+1, ALIGNED_SIZE+2 ... 2*ALIGNED_SIZE-1 ==> 2*ALIGNED_SIZE
#define ALIGNED_SIZEOF(a) (((sizeof(a) + ALIGNED_SIZE - 1) / ALIGNED_SIZE) * ALIGNED_SIZE)


// 数组成员数量
#define ARRAY_MEMBER_NUMBER(array) (sizeof(array)/sizeof(array[0]))

#endif
