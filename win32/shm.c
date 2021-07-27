/* win32/shm.c - shm*() emulation module for hdparm for Windows         */
/*             - by Christian Franke (C) 2006-7 -- freely distributable */

#include "shm.h"

#include <errno.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


int shmget(int key, int size, int flags)
{
	char * addr;
	(void)key; (void)flags;
	addr = VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
	if (!addr) {
		errno = ENOMEM; return -1;
	}
	return (int)addr;
}

int shmctl(int id, int cmd, void *arg)
{
	(void)id; (void)cmd; (void)arg;
	return 0;
}

void * shmat(int id, void *addr, int flags)
{
	(void)addr; (void)flags;
	return (void *)id;
}

int shmdt(void * addr)
{
	if (!VirtualFree(addr, 0, MEM_RELEASE)) {
		errno = EINVAL; return -1;
	}
	return 0;
}
