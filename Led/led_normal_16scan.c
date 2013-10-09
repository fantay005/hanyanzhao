void LedDisplayToScan(int x, int y, int xend, int yend) {
	int vx;
	int *dest, *src;

	for (; y <= yend; ++y) {
		src = __displayBufferBit + y * LED_SCAN_LENGTH + x;
		dest = __scanBufferBit[y % LED_SCAN_MUX] + y / LED_SCAN_MUX + x * 8;
		for (vx = x; vx <= xend; ++vx) {
#if LED_DRIVER_LEVEL==0
			*dest = !(*src++);
#elif LED_DRIVER_LEVEL==1
			*dest = *src++;
#endif
			dest += 8;
		}
	}
}
