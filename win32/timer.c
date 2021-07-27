/* win32/timer.c - sleep/itimer emulation module for hdparm for Windows   */
/*               - by Christian Franke (C) 2006-7 -- freely distributable */

#include "timer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void sleep(int seconds)
{
	Sleep(seconds * 1000L);
}


static struct itimerval start_timerval;
static __int64 start_walltime;

int getitimer(int which, struct itimerval * val)
{
	FILETIME ft; __int64 wallnow, timernow, elapsed;
	(void)which;
	GetSystemTimeAsFileTime(&ft);
	wallnow = (((__int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime);
	elapsed = (wallnow - start_walltime) / 10;
	timernow = (__int64)start_timerval.it_value.tv_sec * 1000000 + start_timerval.it_value.tv_usec - elapsed;
	if (timernow < 0)
		timernow = 0;
	val->it_value.tv_sec  = (long)(timernow / 1000000);
	val->it_value.tv_usec = (long)(timernow % 1000000);
	val->it_interval = start_timerval.it_interval;
	return 0;
}

int setitimer(int which, const struct itimerval *val, struct itimerval *valout)
{
	FILETIME ft;
	(void)which; (void)valout;
	start_timerval = *val;
	GetSystemTimeAsFileTime(&ft);
	start_walltime = (((__int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime);
	return 0;
}
