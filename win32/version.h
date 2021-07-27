/* hdparm for Windows version */
#define VERSION_MAJOR "v6.9"
#define VERSION_MINOR "20070228"

#ifdef __CYGWIN__
#define PLATFORM "Cygwin"
#else
#define PLATFORM "Win32"
#endif

#define VERSION  VERSION_MAJOR"-"VERSION_MINOR" ("PLATFORM")"

