#include "ringbuffer.h"



// void RingBufferAppendData(RingBuffer *ring, char *buf, int len)
// {
// 	int tailLen = &ring->buf[bufferSize] - ring->pin;
// 	if (tailLen < len) {
// 		memcpy(ring->pin, buf, tailLen);
// 		len -= tailLen;
// 		buf += tailLen;
// 		ring->pin = ring->buf;
// 		ring->size += tailLen;
// 	}

// 	memcpy(ring->pin, buf, len);
// 	ring->size += len;
// 	if (ring->size >= ring->bufferSize) {
// 		ring->size = ring->bufferSize;
// 		ring->pout = ring->pin;
// 	}
// }



// void RingBufferGetData(RingBuffer *ring, char *buf, int len)
// {
// }

