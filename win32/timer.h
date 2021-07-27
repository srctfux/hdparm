/* win32/timer.h - sleep/itimer emulation module for hdparm for Windows */
/*               - by Christian Franke (C) 2006 -- freely distributable */

void sleep(int seconds);

#define ITIMER_REAL 0

struct timeval {
	long tv_sec;
	long tv_usec;
};

struct  itimerval {
	struct timeval it_interval;
	struct timeval it_value;
};

int getitimer(int which, struct itimerval * val);
int setitimer(int which, const struct itimerval * val, struct itimerval * valout);

