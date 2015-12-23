#ifndef __CNXIQP_H__
#define __CNXIQP_H__

#define ALIGNED_SIZE 4
#define ALIGNED_SIZEOF(a) (((sizeof(a) + ALIGNED_SIZE - 1) / ALIGNED_SIZE) * ALIGNED_SIZE)
#define ARRAY_MEMBER_NUMBER(array) (sizeof(array)/sizeof(array[0]))

#endif
