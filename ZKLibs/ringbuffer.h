#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <stdbool.h>

typedef struct {
	char *buf;
	char *pin;
	char *pout;
	unsigned int bufferSize;
	unsigned int size; 
} RingBuffer;


//void RingBufferAppendByte(RingBuffer *ring, char dat);
//void RingBufferAppendData(RingBuffer *ring, char *buf, int len);
//void RingBufferGetData(RingBuffer *ring, char *buf, int len);

inline void ringBufferInit(RingBuffer *ring, char *buf, int len)
{
	ring->pin = ring->pout = ring->buf = buf;
	ring->bufferSize = len;
	ring->size = 0;
}

inline void ringBufferAppendByte(RingBuffer *ring, char dat)
{
	*ring->pin++ = dat;
	if (ring->pin >= &ring->buf[ring->bufferSize]) ring->pin = ring->buf;

	if (ring->size >= ring->bufferSize) {
		*ring->pout++;
		if (ring->pout >= &ring->buf[ring->bufferSize]) ring->pout = ring->buf;
		return;
	}
	ring->size++;
}

inline bool ringBufferIsEmpty(RingBuffer *ring) {
	return (ring->size == 0);
}

inline int ringBufferGetByte(RingBuffer *ring)
{
	if (ring->size != 0) {
		unsigned char rc;
		rc = *ring->pout++;
		if (ring->pout >= &ring->buf[ring->bufferSize]) ring->pout = ring->buf;
		ring->size--;
		
		return rc;
	}
	return -1;	
}

#endif
