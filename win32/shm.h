/* win32/shm.h - shm*() emulation module for hdparm for Windows       */
/*             - by Christian Franke (C) 2006 -- freely distributable */

#define IPC_PRIVATE 0
#define IPC_RMID    1
#define SHM_LOCK    2

int shmget(int key, int size, int flags);
int shmctl(int id, int cmd, void *arg);
void * shmat(int id, void *addr, int flags);
int shmdt(void * addr);

