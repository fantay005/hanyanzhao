#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

static char __needReset = 0;
void WatchdogInit();
void WatchdogStopFeed();
void WatchdogFeed();

#endif
