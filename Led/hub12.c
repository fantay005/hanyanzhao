#include "hub12.h"

void LedDisplayToScan(int x, int y, int xend, int yend) {
	int vx;
	unsigned int dest;
	const unsigned short *pOffset;
	int *src;

	for (; y <= yend; ++y) {
		src = __displayBufferBit + y * ARRAY_MEMBER_NUMBER(__displayBuffer[0]) + x;
		dest = (unsigned int)__scanBufferBit[y % LED_SCAN_MUX];
		pOffset = &__addrTransferTable[y % 16][x];
		for (vx = x; vx <= xend; ++vx) {
#if LED_DRIVER_LEVEL==0
			*((int *)(dest + *pOffset++)) = !(*src++);
#elif LED_DRIVER_LEVEL==1
			*((int *)(dest + *pOffset++)) = *src++;
#else
#error "LED_DRIVER_LEVEL MUST be 0 or 1"
#endif
		}
	}
}
