#ifndef __RINGBUFFER_HH__
#define __RINGBUFFER_HH__

#include <stdbool.h>

typedef struct {
	char *buf;
	char *pin;
	char *pout;
	char *pend;

	unsigned int bufferSize;
	unsigned int size;
} RingBuffer;


//void RingBufferAppendByte(RingBuffer *ring, char dat);
//void RingBufferAppendData(RingBuffer *ring, char *buf, int len);
//void RingBufferGetData(RingBuffer *ring, char *buf, int len);

inline void RingBufferInit(RingBuffer *ring, char *buf, int len) {
	ring->pin = ring->pout = ring->buf = buf;
	ring->bufferSize = len;
	ring->size = 0;
}

inline void RingBufferAppendByte(RingBuffer *ring, char dat) {
	*ring->pin++ = dat;
	if (ring->pin >= &ring->buf[ring->bufferSize]) {
		ring->pin = ring->buf;
	}

	if (ring->size >= ring->bufferSize) {
		++ring->pout;
		if (ring->pout >= &ring->buf[ring->bufferSize]) {
			ring->pout = ring->buf;
		}
		return;
	}
	ring->size++;
}

inline bool RingBufferIsEmpty(RingBuffer *ring) {
	return (ring->size == 0);
}

inline int RingBufferGetByte(RingBuffer *ring) {
	if (ring->size != 0) {
		unsigned char rc;
		rc = *ring->pout++;
		if (ring->pout >= &ring->buf[ring->bufferSize]) {
			ring->pout = ring->buf;
		}
		ring->size--;

		return rc;
	}
	return -1;
}

#endif
