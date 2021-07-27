/* win32/rawio.h - ioctl() emulation module for hdparm for Windows        */
/*               - by Christian Franke (C) 2006-7 -- freely distributable */

int win32_open(const char * name, unsigned flags, unsigned perm);
int win32_close(int fd);
int win32_read(int fd, char * buf, int size);
long win32_lseek(int fd, long offset, int where);
int win32_ioctl(int fd, int code, void * arg);

extern int win32_debug;

#ifndef O_DIRECT
#define O_DIRECT	040000
#endif

#ifndef ENOMSG
#define ENOMSG		100
#endif

#ifndef RAWIO_INTERNAL

#define open(name, flags)        win32_open(name, flags, 0666)
#define close(fd)                win32_close(fd)
#define read(fd, buf, size)      win32_read(fd, buf, size)
#define lseek(fd, offset, where) win32_lseek(fd, offset, where)
#define ioctl(fd, code, arg)     win32_ioctl(fd, code, (void*)(arg))
#define fsync(fd) ((void)0)
#define sync()    ((void)0)

#endif
