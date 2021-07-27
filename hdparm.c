/* hdparm.c - Command line interface to get/set hard disk parameters */
/*          - by Mark Lord (C) 1994-2005 -- freely distributable */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#endif
#if !defined(_WIN32) && !defined(__CYGWIN__)
#include <endian.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <sys/sysmacros.h>
#include <sys/times.h>
#include <sys/mount.h>
#include <linux/types.h>
#include <linux/hdreg.h>
#include <linux/major.h>
#include <asm/byteorder.h>
//#include <endian.h>
#else
#ifndef __CYGWIN__
#include "win32/timer.h"
#endif
#include "win32/fs.h"
#include "win32/hdreg.h"
#include "win32/shm.h"
#include "win32/rawio.h"
#include "win32/version.h"
#define __le16_to_cpus(x) ((void)(x))
#endif

#include "hdparm.h"

extern const char *minor_str[];

#ifndef VERSION
#define VERSION "v6.9"
#endif

#undef DO_FLUSHCACHE		/* under construction: force cache flush on -W0 */

#ifndef O_DIRECT
#define O_DIRECT	040000	/* direct disk access, not easily obtained from headers */
#endif

#if !defined(_WIN32) && !defined(__CYGWIN__)
#ifndef CDROM_SELECT_SPEED	/* already defined in 2.3.xx kernels and above */
#define CDROM_SELECT_SPEED	0x5322
#endif
#endif

#ifndef BLKGETSIZE64
#define BLKGETSIZE64		_IOR(0x12,114,size_t)
#endif

#define TIMING_BUF_MB		2
#define TIMING_BUF_BYTES	(TIMING_BUF_MB * 1024 * 1024)

char *progname;
static int verbose = 0, noisy = 1, quiet = 0;
static int flagcount = 0, do_flush = 0;
static int do_ctimings, do_timings = 0;
static double ctimings = 0.0, timings = 0.0;

#ifdef HDIO_GET_IDENTITY
static int get_identity = 0;
#endif
#ifdef HDIO_GETGEO
static int get_geom = 0;
#endif

#ifdef BLKRASET
static unsigned long set_fsreadahead= 0, get_fsreadahead= 0, fsreadahead= 0;
#endif
#ifdef BLKROSET
static unsigned long set_readonly = 0, get_readonly = 0, readonly = 0;
#endif
#ifdef HDIO_SET_UNMASKINTR
static unsigned long set_unmask   = 0, get_unmask   = 0, unmask   = 0;
#endif
#ifdef HDIO_SET_MULTCOUNT
static unsigned long set_mult     = 0, get_mult     = 0, mult     = 0;
#endif
#ifdef HDIO_SET_DMA
static unsigned long set_dma      = 0, get_dma      = 0, dma      = 0;
#endif
#ifdef HDIO_GET_QDMA
static unsigned long set_dma_q	  = 0, get_dma_q    = 0, dma_q	  = 0;
#endif
#ifdef HDIO_SET_NOWERR
static unsigned long set_nowerr   = 0, get_nowerr   = 0, nowerr   = 0;
#endif
#ifdef HDIO_GET_KEEPSETTINGS
static unsigned long set_keep     = 0, get_keep     = 0, keep     = 0;
#endif
#ifdef HDIO_SET_32BIT
static unsigned long set_io32bit  = 0, get_io32bit  = 0, io32bit  = 0;
#endif
#ifdef HDIO_SET_PIO_MODE
static unsigned long set_piomode  = 0, noisy_piomode= 0;
int piomode = 0;
#endif
#ifdef HDIO_DRIVE_CMD
static unsigned long set_dkeep    = 0, get_dkeep    = 0, dkeep    = 0;
static unsigned long set_standby  = 0, get_standby  = 0, standby_requested= 0;
static unsigned long set_xfermode = 0, get_xfermode = 0;
static int xfermode_requested= 0;
static unsigned long set_lookahead= 0, get_lookahead= 0, lookahead= 0;
static unsigned long set_prefetch = 0, get_prefetch = 0, prefetch = 0;
static unsigned long set_defects  = 0, get_defects  = 0, defects  = 0;
static unsigned long set_wcache   = 0, get_wcache   = 0, wcache   = 0;
static unsigned long set_doorlock = 0, get_doorlock = 0, doorlock = 0;
static unsigned long set_seagate  = 0, get_seagate  = 0;
static unsigned long set_standbynow = 0, get_standbynow = 0;
static unsigned long set_sleepnow   = 0, get_sleepnow   = 0;
static unsigned long set_powerup_in_standby = 0, get_powerup_in_standby = 0, powerup_in_standby = 0;
static unsigned long get_hitachi_temp = 0;

static unsigned long set_freeze   = 0;
#ifndef WIN_SECURITY_FREEZE_LOCK
	#define WIN_SECURITY_FREEZE_LOCK	0xF5
#endif

#ifdef IDE_DRIVE_TASK_NO_DATA
static unsigned long security_master = 1, security_mode = 0;
static unsigned long enhanced_erase = 0;
static unsigned long set_security   = 0;

#ifndef WIN_SECURITY_SET_PASS
	#define WIN_SECURITY_SET_PASS		0xF1
	#define WIN_SECURITY_UNLOCK		0xF2
	#define WIN_SECURITY_ERASE_PREPARE	0xF3
	#define WIN_SECURITY_ERASE_UNIT		0xF4
	#define WIN_SECURITY_DISABLE		0xF6
#endif
static unsigned int security_command = WIN_SECURITY_UNLOCK;

static char security_password[33];
#endif // IDE_DRIVE_TASK_NO_DATA

static unsigned long get_powermode  = 0;
static unsigned long set_apmmode = 0, get_apmmode= 0, apmmode = 0;
#endif // HDIO_DRIVE_CMD
#ifdef CDROM_SELECT_SPEED
static unsigned long set_cdromspeed = 0, cdromspeed = 0;
#endif /* CDROM_SELECT_SPEED */
#ifdef HDIO_DRIVE_CMD
static int get_IDentity = 0;
#endif
#ifdef HDIO_UNREGISTER_HWIF
static int	unregister_hwif = 0;
static int	hwif = 0;
#endif
#ifdef HDIO_SCAN_HWIF
static int	scan_hwif = 0;
static int	hwif_data = 0;
static int	hwif_ctrl = 0;
static int	hwif_irq = 0;
#endif
#ifdef HDIO_GET_BUSSTATE
static int	set_busstate = 0, get_busstate = 0, busstate = 0;
#endif
#ifdef BLKRRPART
static int	reread_partn = 0;
#endif
#if defined(HDIO_GET_ACOUSTIC) || defined(HDIO_DRIVE_CMD)
static int	set_acoustic = 0, get_acoustic = 0, acoustic = 0;
#endif

#ifdef HDIO_DRIVE_RESET
static int	perform_reset = 0;
#endif /* HDIO_DRIVE_RESET */
#ifdef HDIO_TRISTATE_HWIF
static int	perform_tristate = 0,	tristate = 0;
#endif /* HDIO_TRISTATE_HWIF */

#ifdef O_NONBLOCK
static int open_flags = O_RDONLY|O_NONBLOCK;
#else
static int open_flags = O_RDONLY;
#endif

#ifdef HDIO_GET_IDENTITY
// Historically, if there was no HDIO_OBSOLETE_IDENTITY, then
// then the HDIO_GET_IDENTITY only returned 142 bytes.
// Otherwise, HDIO_OBSOLETE_IDENTITY returns 142 bytes,
// and HDIO_GET_IDENTITY returns 512 bytes.  But the latest
// 2.5.xx kernels no longer define HDIO_OBSOLETE_IDENTITY
// (which they should, but they should just return -EINVAL).
//
// So.. we must now assume that HDIO_GET_IDENTITY returns 512 bytes.
// On a really old system, it will not, and we will be confused.
// Too bad, really.

const char *cfg_str[] =
{	"",	        " HardSect",   " SoftSect",  " NotMFM",
	" HdSw>15uSec", " SpinMotCtl", " Fixed",     " Removeable",
	" DTR<=5Mbs",   " DTR>5Mbs",   " DTR>10Mbs", " RotSpdTol>.5%",
	" dStbOff",     " TrkOff",     " FmtGapReq", " nonMagnetic"
};

const char *SlowMedFast[]	= {"slow", "medium", "fast", "eide", "ata"};
const char *BuffType[]	= {"unknown", "1Sect", "DualPort", "DualPortCache"};

#define YN(b)	(((b)==0)?"no":"yes")

static void dmpstr (const char *prefix, unsigned int i, const char *s[], unsigned int maxi)
{
	if (i > maxi)
		printf("%s%u", prefix, i);
	else
		printf("%s%s", prefix, s[i]);
}

static void dump_identity (const struct hd_driveid *id)
{
	int i;
	char pmodes[64] = {0,}, dmodes[128]={0,}, umodes[128]={0,};
	const unsigned short int *id_regs= (const void*) id;
	unsigned long capacity;

	printf("\n Model=%.40s, FwRev=%.8s, SerialNo=%.20s",
		id->model, id->fw_rev, id->serial_no);
	printf("\n Config={");
	for (i=0; i<=15; i++) {
		if (id->config & (1<<i))
			printf("%s", cfg_str[i]);
	}
	printf(" }\n");
	printf(" RawCHS=%u/%u/%u, TrkSize=%u, SectSize=%u, ECCbytes=%u\n",
		id->cyls, id->heads, id->sectors,
		id->track_bytes, id->sector_bytes, id->ecc_bytes);
	dmpstr(" BuffType=",id->buf_type,BuffType,3);
	printf(", BuffSize=%ukB, MaxMultSect=%u", id->buf_size/2, id->max_multsect);
	if (id->max_multsect) {
		printf(", MultSect=");
		if (!(id->multsect_valid&1))
			printf("?%u?", id->multsect);
		else if (id->multsect)
			printf("%u", id->multsect);
		else
			printf("off");
	}
	putchar('\n');
	if (id->tPIO <= 5) {
		strcat(pmodes, "pio0 ");
		if (id->tPIO >= 1) strcat(pmodes, "pio1 ");
		if (id->tPIO >= 2) strcat(pmodes, "pio2 ");
	}
	if (!(id->field_valid&1))
		printf(" (maybe):");
	capacity = (id->cur_capacity1 << 16) | id->cur_capacity0;
	printf(" CurCHS=%u/%u/%u, CurSects=%lu", id->cur_cyls, id->cur_heads, id->cur_sectors, capacity);
	printf(", LBA=%s", YN(id->capability&2));
	if (id->capability&2)
 		printf(", LBAsects=%u", id->lba_capacity);

	if (id->capability&1) {
		if (id->dma_1word | id->dma_mword) {
			if (id->dma_1word & 0x100)	strcat(dmodes,"*");
			if (id->dma_1word & 1)		strcat(dmodes,"sdma0 ");
			if (id->dma_1word & 0x200)	strcat(dmodes,"*");
			if (id->dma_1word & 2)		strcat(dmodes,"sdma1 ");
			if (id->dma_1word & 0x400)	strcat(dmodes,"*");
			if (id->dma_1word & 4)		strcat(dmodes,"sdma2 ");
			if (id->dma_1word & 0xf800)	strcat(dmodes,"*");
			if (id->dma_1word & 0xf8)	strcat(dmodes,"sdma? ");
			if (id->dma_mword & 0x100)	strcat(dmodes,"*");
			if (id->dma_mword & 1)		strcat(dmodes,"mdma0 ");
			if (id->dma_mword & 0x200)	strcat(dmodes,"*");
			if (id->dma_mword & 2)		strcat(dmodes,"mdma1 ");
			if (id->dma_mword & 0x400)	strcat(dmodes,"*");
			if (id->dma_mword & 4)		strcat(dmodes,"mdma2 ");
			if (id->dma_mword & 0xf800)	strcat(dmodes,"*");
			if (id->dma_mword & 0xf8)	strcat(dmodes,"mdma? ");
		}
	}
	printf("\n IORDY=");
	if (id->capability&8)
		printf((id->capability&4) ? "on/off" : "yes");
	else
		printf("no");
	if ((id->capability&8) || (id->field_valid&2)) {
		if (id->field_valid&2) {
			printf(", tPIO={min:%u,w/IORDY:%u}", id->eide_pio, id->eide_pio_iordy);
			if (id->eide_pio_modes & 1)	strcat(pmodes, "pio3 ");
			if (id->eide_pio_modes & 2)	strcat(pmodes, "pio4 ");
			if (id->eide_pio_modes &~3)	strcat(pmodes, "pio? ");
		}
		if (id->field_valid&4) {
			if (id->dma_ultra & 0x100)	strcat(umodes,"*");
			if (id->dma_ultra & 0x001)	strcat(umodes,"udma0 ");
			if (id->dma_ultra & 0x200)	strcat(umodes,"*");
			if (id->dma_ultra & 0x002)	strcat(umodes,"udma1 ");
			if (id->dma_ultra & 0x400)	strcat(umodes,"*");
			if (id->dma_ultra & 0x004)	strcat(umodes,"udma2 ");
			if (id->dma_ultra & 0x800)	strcat(umodes,"*");
			if (id->dma_ultra & 0x008)	strcat(umodes,"udma3 ");
			if (id->dma_ultra & 0x1000)	strcat(umodes,"*");
			if (id->dma_ultra & 0x010)	strcat(umodes,"udma4 ");
			if (id->dma_ultra & 0x2000)	strcat(umodes,"*");
			if (id->dma_ultra & 0x020)	strcat(umodes,"udma5 ");
			if (id->dma_ultra & 0x4000)	strcat(umodes,"*");
			if (id->dma_ultra & 0x0040)	strcat(umodes,"udma6 ");
			if (id->dma_ultra & 0x8000)	strcat(umodes,"*");
			if (id->dma_ultra & 0x0080)	strcat(umodes,"udma7 ");
		}
	}
	if ((id->capability&1) && (id->field_valid&2))
		printf(", tDMA={min:%u,rec:%u}", id->eide_dma_min, id->eide_dma_time);
	printf("\n PIO modes:  %s", pmodes);
	if (*dmodes)
		printf("\n DMA modes:  %s", dmodes);
	if (*umodes)
		printf("\n UDMA modes: %s", umodes);

	printf("\n AdvancedPM=%s",YN(id_regs[83]&8));
	if (id_regs[83] & 8) {
		if (!(id_regs[86]&8))
			printf(": disabled (255)");
		else if ((id_regs[91]&0xFF00)!=0x4000)
			printf(": unknown setting");
		else
			printf(": mode=0x%02X (%u)",id_regs[91]&0xFF,id_regs[91]&0xFF);
	}
	if (id_regs[82]&0x20)
		printf(" WriteCache=%s",(id_regs[85]&0x20) ? "enabled" : "disabled");
#ifdef __NEW_HD_DRIVE_ID
	if (id->minor_rev_num || id->major_rev_num) {
		printf("\n Drive conforms to: ");
		if (id->minor_rev_num <= 31)
			printf("%s: ", minor_str[id->minor_rev_num]);
		else
			printf("unknown: ");
		if (id->major_rev_num != 0x0000 &&  /* NOVAL_0 */
		    id->major_rev_num != 0xFFFF) {  /* NOVAL_1 */
			/* through ATA/ATAPI-7 is currently defined--
			 * increase this value as further specs are 
			 * standardized (though we can guess safely to 15)
			 */
			for (i=0; i <= 7; i++) {
				if (id->major_rev_num & (1<<i))
					printf(" ATA/ATAPI-%u", i);
			}
		}
	}
#endif /* __NEW_HD_DRIVE_ID */
	printf("\n");
	printf("\n * signifies the current active mode\n");
	printf("\n");
}

#endif /* HDIO_GET_IDENTITY */

void flush_buffer_cache (int fd)
{
	fsync (fd);				/* flush buffers */
#ifdef BLKFLSBUF
	if (ioctl(fd, BLKFLSBUF, NULL))		/* do it again, big time */
		perror("BLKFLSBUF failed");
#endif
#ifdef HDIO_DRIVE_CMD
	if (ioctl(fd, HDIO_DRIVE_CMD, NULL) && errno != EINVAL)	/* await completion */
		perror("HDIO_DRIVE_CMD(null) (wait for flush complete) failed");
#endif
}

int seek_to_zero (int fd)
{
	if (lseek(fd, (off_t) 0, SEEK_SET)) {
		perror("lseek() failed");
		return 1;
	}
	return 0;
}

int read_big_block (int fd, char *buf)
{
	int i, rc;
	if ((rc = read(fd, buf, TIMING_BUF_BYTES)) != TIMING_BUF_BYTES) {
		if (rc) {
			if (rc == -1)
				perror("read() failed");
			else
				fprintf(stderr, "read(%u) returned %u bytes\n", TIMING_BUF_BYTES, rc);
		} else {
			fputs ("read() hit EOF - device too small\n", stderr);
		}
		return 1;
	}
	/* access all sectors of buf to ensure the read fully completed */
	for (i = 0; i < TIMING_BUF_BYTES; i += 512)
		buf[i] &= 1;
	return 0;
}

void time_cache (int fd)
{
	char *buf;
	struct itimerval e1, e2;
	int shmid;
	double elapsed, elapsed2;
	unsigned int iterations, total_MB;

	if ((shmid = shmget(IPC_PRIVATE, TIMING_BUF_BYTES, 0600)) == -1) {
		perror ("could not allocate sharedmem buf");
		return;
	}
	if (shmctl(shmid, SHM_LOCK, NULL) == -1) {
		perror ("could not lock sharedmem buf");
		(void) shmctl(shmid, IPC_RMID, NULL);
		return;
	}
	if ((buf = shmat(shmid, (char *) 0, 0)) == (char *) -1) {
		perror ("could not attach sharedmem buf");
		(void) shmctl(shmid, IPC_RMID, NULL);
		return;
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
		perror ("shmctl(,IPC_RMID,) failed");

	/* Clear out the device request queues & give them time to complete */
	sync();
	sleep(3);

	/*
	 * getitimer() is used rather than gettimeofday() because
	 * it is much more consistent (on my machine, at least).
	 */
	{
		const struct itimerval tv = {{1000,0},{1000,0}};
		setitimer(ITIMER_REAL, &tv, NULL);
	}
	if (seek_to_zero (fd)) return;
	if (read_big_block (fd, buf)) return;
	printf(" Timing %scached reads:   ", (open_flags & O_DIRECT) ? "O_DIRECT " : "");
	fflush(stdout);

	/* Clear out the device request queues & give them time to complete */
	sync();
	sleep(1);

	/* Now do the timing */
	iterations = 0;
	getitimer(ITIMER_REAL, &e1);
	do {
		++iterations;
		if (seek_to_zero (fd) || read_big_block (fd, buf))
			goto quit;
		getitimer(ITIMER_REAL, &e2);
		elapsed = (e1.it_value.tv_sec - e2.it_value.tv_sec)
		 + ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);
	} while (elapsed < ctimings);
	total_MB = iterations * TIMING_BUF_MB;

	elapsed = (e1.it_value.tv_sec - e2.it_value.tv_sec)
	 + ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);

	/* Now remove the lseek() and getitimer() overheads from the elapsed time */
	getitimer(ITIMER_REAL, &e1);
	do {
		if (seek_to_zero (fd))
			goto quit;
		getitimer(ITIMER_REAL, &e2);
		elapsed2 = (e1.it_value.tv_sec - e2.it_value.tv_sec)
		 + ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);
	} while (--iterations);

	elapsed -= elapsed2;

	if (total_MB >= elapsed)  /* more than 1MB/s */
		printf("%3u MB in %5.2f seconds = %6.2f MB/sec\n",
			total_MB, elapsed,
			total_MB / elapsed);
	else
		printf("%3u MB in %5.2f seconds = %6.2f kB/sec\n",
			total_MB, elapsed,
			total_MB / elapsed * 1024);

	flush_buffer_cache(fd);
	sleep(1);
quit:
	if (-1 == shmdt(buf))
		perror ("could not detach sharedmem buf");
}

static int do_blkgetsize (int fd, __ull *blksize64)
{
	int		rc;
	unsigned int	blksize32 = 0;

	if (0 == ioctl(fd, BLKGETSIZE64, blksize64)) {	// returns bytes
		*blksize64 /= 512;
		return 0;
	}
	rc = ioctl(fd, BLKGETSIZE, &blksize32);	// returns sectors
	if (rc)
		perror(" BLKGETSIZE failed");
	*blksize64 = blksize32;
	return rc;
}

void time_device (int fd)
{
	char *buf;
	double elapsed;
	struct itimerval e1, e2;
	int shmid;
	unsigned int max_iterations = 1024, total_MB, iterations;

	//
	// get device size
	//
	if (do_ctimings || do_timings) {
		__ull blksize;
		if (0 == do_blkgetsize(fd, &blksize))
			max_iterations = (unsigned)(blksize / (2 * 1024) / TIMING_BUF_MB);
	}

	if ((shmid = shmget(IPC_PRIVATE, TIMING_BUF_BYTES, 0600)) == -1) {
		perror ("could not allocate sharedmem buf");
		return;
	}
	if (shmctl(shmid, SHM_LOCK, NULL) == -1) {
		perror ("could not lock sharedmem buf");
		(void) shmctl(shmid, IPC_RMID, NULL);
		return;
	}
	if ((buf = shmat(shmid, (char *) 0, 0)) == (char *) -1) {
		perror ("could not attach sharedmem buf");
		(void) shmctl(shmid, IPC_RMID, NULL);
		return;
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
		perror ("shmctl(,IPC_RMID,) failed");

	/* Clear out the device request queues & give them time to complete */
	sync();
	sleep(3);

	printf(" Timing %s disk reads:  ", (open_flags & O_DIRECT) ? "O_DIRECT" : "buffered");
	fflush(stdout);

	/*
	 * getitimer() is used rather than gettimeofday() because
	 * it is much more consistent (on my machine, at least).
	 */
	{
		const struct itimerval tv = {{1000,0},{1000,0}};
		setitimer(ITIMER_REAL, &tv, NULL);
	}

	/* Now do the timings for real */
	iterations = 0;
	getitimer(ITIMER_REAL, &e1);
	do {
		++iterations;
		if (read_big_block (fd, buf))
			goto quit;
		getitimer(ITIMER_REAL, &e2);
		elapsed = (e1.it_value.tv_sec - e2.it_value.tv_sec)
		 + ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);
	} while (elapsed < timings && iterations < max_iterations);

	total_MB = iterations * TIMING_BUF_MB;
	if ((total_MB / elapsed) > 1.0)  /* more than 1MB/s */
		printf("%3u MB in %5.2f seconds = %6.2f MB/sec\n",
			total_MB, elapsed, total_MB / elapsed);
	else
		printf("%3u MB in %5.2f seconds = %6.2f kB/sec\n",
			total_MB, elapsed, total_MB / elapsed * 1024);
quit:
	if (-1 == shmdt(buf))
		perror ("could not detach sharedmem buf");
}

static void on_off (unsigned int value)
{
	printf(value ? " (on)\n" : " (off)\n");
}

#ifdef HDIO_GET_BUSSTATE
static void bus_state_value (unsigned int value)
{
	const char *string;

	switch (value) {
	case BUSSTATE_ON:
		string = " (on)\n";
		break;
	case BUSSTATE_OFF:
		string = " (off)\n";
		break;
	case BUSSTATE_TRISTATE:
		string = " (tristate)\n";
		break;
	default:
		string = " (unknown: %d)\n";
		break;
	}
	printf(string, value);
}
#endif

#ifdef HDIO_DRIVE_CMD
static void interpret_standby (unsigned int standby)
{
	printf(" (");
	switch(standby) {
		case 0:		printf("off");
				break;
		case 252:	printf("21 minutes");
				break;
		case 253:	printf("vendor-specific");
				break;
		case 254:	printf("?reserved");
				break;
		case 255:	printf("21 minutes + 15 seconds");
				break;
		default:
			if (standby <= 240) {
				unsigned int secs = standby * 5;
				unsigned int mins = secs / 60;
				secs %= 60;
				if (mins)	  printf("%u minutes", mins);
				if (mins && secs) printf(" + ");
				if (secs)	  printf("%u seconds", secs);
			} else if (standby <= 251) {
				unsigned int mins = (standby - 240) * 30;
				unsigned int hrs  = mins / 60;
				mins %= 60;
				if (hrs)	  printf("%u hours", hrs);
				if (hrs && mins)  printf(" + ");
				if (mins)	  printf("%u minutes", mins);
			} else
				printf("illegal value");
			break;
	}
	printf(")\n");
}

struct xfermode_entry {
	int val;
	const char *name;
};

static const struct xfermode_entry xfermode_table[] = {
	{ 8,    "pio0" },
	{ 9,    "pio1" },
	{ 10,   "pio2" },
	{ 11,   "pio3" },
	{ 12,   "pio4" },
	{ 13,   "pio5" },
	{ 14,   "pio6" },
	{ 15,   "pio7" },
	{ 16,   "sdma0" },
	{ 17,   "sdma1" },
	{ 18,   "sdma2" },
	{ 19,   "sdma3" },
	{ 20,   "sdma4" },
	{ 21,   "sdma5" },
	{ 22,   "sdma6" },
	{ 23,   "sdma7" },
	{ 32,   "mdma0" },
	{ 33,   "mdma1" },
	{ 34,   "mdma2" },
	{ 35,   "mdma3" },
	{ 36,   "mdma4" },
	{ 37,   "mdma5" },
	{ 38,   "mdma6" },
	{ 39,   "mdma7" },
	{ 64,   "udma0" },
	{ 65,   "udma1" },
	{ 66,   "udma2" },
	{ 67,   "udma3" },
	{ 68,   "udma4" },
	{ 69,   "udma5" },
	{ 70,   "udma6" },
	{ 71,   "udma7" },
	{ 0, NULL }
};

static int translate_xfermode(char * name)
{
	const struct xfermode_entry *tmp;
	char *endptr;
	int val = -1;

	for (tmp = xfermode_table; tmp->name != NULL; ++tmp) {
		if (!strcmp(name, tmp->name))
			return tmp->val;
	}
	val = strtol(name, &endptr, 10);
	if (*endptr == '\0')
		return val;
	return -1;
}

static void interpret_xfermode (unsigned int xfermode)
{
	printf(" (");
	switch(xfermode) {
		case 0:		printf("default PIO mode");
				break;
		case 1:		printf("default PIO mode, disable IORDY");
				break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:	printf("PIO flow control mode%u", xfermode-8);
				break;
		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:	printf("singleword DMA mode%u", xfermode-16);
				break;
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
		case 38:
		case 39:	printf("multiword DMA mode%u", xfermode-32);
				break;
		case 64:
		case 65:
		case 66:
		case 67:
		case 68:
		case 69:
		case 70:
		case 71:	printf("UltraDMA mode%u", xfermode-64);
				break;
		default:
				printf("unknown, probably not valid");
				break;
	}
	printf(")\n");
}
#endif /* HDIO_DRIVE_CMD */

#ifndef VXVM_MAJOR
#define VXVM_MAJOR 199
#endif

#ifndef CCISS_MAJOR
#define CCISS_MAJOR 104
#endif

void process_dev (char *devname)
{
	int fd;
	static long parm, multcount;
#ifndef HDIO_DRIVE_CMD
	int force_operation = 0;
#endif

	fd = open (devname, open_flags);
	if (fd < 0) {
		perror(devname);
		exit(errno);
	}
	if (!quiet)
		printf("\n%s:\n", devname);

#ifdef BLKRASET
	if (set_fsreadahead) {
		if (get_fsreadahead)
			printf(" setting fs readahead to %ld\n", fsreadahead);
		if (ioctl(fd, BLKRASET, fsreadahead))
			perror(" BLKRASET failed");
	}
#endif
#ifdef HDIO_UNREGISTER_HWIF
	if (unregister_hwif) {
		printf(" attempting to unregister hwif#%u\n", hwif);
		if (ioctl(fd, HDIO_UNREGISTER_HWIF, hwif))
			perror(" HDIO_UNREGISTER_HWIF failed");
	}
#endif
#ifdef HDIO_SCAN_HWIF
	if (scan_hwif) {
		int	args[3];
		printf(" attempting to scan hwif (0x%x, 0x%x, %u)\n", hwif_data, hwif_ctrl, hwif_irq);
		args[0] = hwif_data;
		args[1] = hwif_ctrl;
		args[2] = hwif_irq;
		if (ioctl(fd, HDIO_SCAN_HWIF, args))
			perror(" HDIO_SCAN_HWIF failed");
	}
#endif
#ifdef HDIO_SET_PIO_MODE
	if (set_piomode) {
		if (noisy_piomode) {
			if (piomode == 255)
				printf(" attempting to auto-tune PIO mode\n");
			else if (piomode < 100)
				printf(" attempting to set PIO mode to %d\n", piomode);
			else if (piomode < 200)
				printf(" attempting to set MDMA mode to %d\n", (piomode-100));
			else
				printf(" attempting to set UDMA mode to %d\n", (piomode-200));
		}
		if (ioctl(fd, HDIO_SET_PIO_MODE, piomode))
			perror(" HDIO_SET_PIO_MODE failed");
	}
#endif
#ifdef HDIO_SET_32BIT
	if (set_io32bit) {
		if (get_io32bit)
			printf(" setting 32-bit IO_support flag to %ld\n", io32bit);
		if (ioctl(fd, HDIO_SET_32BIT, io32bit))
			perror(" HDIO_SET_32BIT failed");
	}
#endif
#ifdef HDIO_SET_MULTCOUNT
	if (set_mult) {
		if (get_mult)
			printf(" setting multcount to %ld\n", mult);
		if (ioctl(fd, HDIO_SET_MULTCOUNT, mult))
			perror(" HDIO_SET_MULTCOUNT failed");
#ifndef HDIO_DRIVE_CMD
		else force_operation = 1;
#endif
	}
#endif
#ifdef BLKROSET
	if (set_readonly) {
		if (get_readonly) {
			printf(" setting readonly to %ld", readonly);
			on_off(readonly);
		}
		if (ioctl(fd, BLKROSET, &readonly))
			perror(" BLKROSET failed");
	}
#endif
#ifdef HDIO_SET_UNMASKINTR
	if (set_unmask) {
		if (get_unmask) {
			printf(" setting unmaskirq to %ld", unmask);
			on_off(unmask);
		}
		if (ioctl(fd, HDIO_SET_UNMASKINTR, unmask))
			perror(" HDIO_SET_UNMASKINTR failed");
	}
#endif
#ifdef HDIO_SET_DMA
	if (set_dma) {
		if (get_dma) {
			printf(" setting using_dma to %ld", dma);
			on_off(dma);
		}
		if (ioctl(fd, HDIO_SET_DMA, dma))
			perror(" HDIO_SET_DMA failed");
	}
#endif
#ifdef HDIO_SET_QDMA
	if (set_dma_q) {
		if (get_dma_q) {
			printf(" setting DMA queue_depth to %ld", dma_q);
			on_off(dma_q);
		}
		if (ioctl(fd, HDIO_SET_QDMA, dma_q))
			perror(" HDIO_SET_QDMA failed");
	}
#endif
#ifdef HDIO_SET_NOWERR
	if (set_nowerr) {
		if (get_nowerr) {
			printf(" setting nowerr to %ld", nowerr);
			on_off(nowerr);
		}
		if (ioctl(fd, HDIO_SET_NOWERR, nowerr))
			perror(" HDIO_SET_NOWERR failed");
	}
#endif
#ifdef HDIO_GET_KEEPSETTINGS
	if (set_keep) {
		if (get_keep) {
			printf(" setting keep_settings to %ld", keep);
			on_off(keep);
		}
		if (ioctl(fd, HDIO_SET_KEEPSETTINGS, keep))
			perror(" HDIO_SET_KEEPSETTINGS failed");
	}
#endif /* HDIO_GET_KEEPSETTINGS */
#ifdef HDIO_DRIVE_CMD
	if (set_doorlock) {
		unsigned char args[4] = {0,0,0,0};
		args[0] = doorlock ? WIN_DOORLOCK : WIN_DOORUNLOCK;
		if (get_doorlock) {
			printf(" setting drive doorlock to %ld", doorlock);
			on_off(doorlock);
		}
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(doorlock) failed");
	}
	if (set_dkeep) {
		/* lock/unlock the drive's "feature" settings */
		unsigned char args[4] = {WIN_SETFEATURES,0,0,0};
		if (get_dkeep) {
			printf(" setting drive keep features to %ld", dkeep);
			on_off(dkeep);
		}
		args[2] = dkeep ? 0x66 : 0xcc;
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(keepsettings) failed");
	}
	if (set_defects) {
		unsigned char args[4] = {WIN_SETFEATURES,0,0x04,0};
		args[2] = defects ? 0x04 : 0x84;
		if (get_defects)
			printf(" setting drive defect management to %ld\n", defects);
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(defectmgmt) failed");
	}
	if (set_prefetch) {
		unsigned char args[4] = {WIN_SETFEATURES,0,0xab,0};
		args[1] = (char)prefetch;
		if (get_prefetch)
			printf(" setting drive prefetch to %ld\n", prefetch);
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(setprefetch) failed");
	}
	if (set_xfermode) {
		unsigned char args[4] = {WIN_SETFEATURES,0,3,0};
		args[1] = xfermode_requested;
		if (get_xfermode) {
			printf(" setting xfermode to %d", xfermode_requested);
			interpret_xfermode(xfermode_requested);
		}
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(setxfermode) failed");
	}
	if (set_lookahead) {
		unsigned char args[4] = {WIN_SETFEATURES,0,0,0};
		args[2] = lookahead ? 0xaa : 0x55;
		if (get_lookahead) {
			printf(" setting drive read-lookahead to %ld", lookahead);
			on_off(lookahead);
		}
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(setreadahead) failed");
	}
	if (set_powerup_in_standby) {
		unsigned char args[4] = {WIN_SETFEATURES,0,0,0};
		args[2] = powerup_in_standby ? 0x06 : 0x86;
		if (get_powerup_in_standby) {
			printf(" setting power-up in standby to %ld", powerup_in_standby);
			on_off(powerup_in_standby);
		}
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(powerup_in_standby) failed");
	}
	if (set_apmmode) {
		unsigned char args[4] = {WIN_SETFEATURES,0,0,0};
		if (apmmode<1) apmmode=1;
		if (apmmode>255) apmmode=255;
		if (get_apmmode)
			printf(" setting Advanced Power Management level to");
		if (apmmode==255) {
			/* disable Advanced Power Management */
			args[2] = 0x85; /* feature register */
			if (get_apmmode) printf(" disabled\n");
		} else {
			/* set Advanced Power Management mode */
			args[2] = 0x05; /* feature register */
			args[1] = (unsigned char)apmmode; /* sector count register */
			if (get_apmmode) printf(" 0x%02lX (%ld)\n",apmmode,apmmode);
		}
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD failed");
	}
#endif
#ifdef CDROM_SELECT_SPEED
	if (set_cdromspeed) {
		printf ("setting cdrom speed to %ld\n", cdromspeed);
		if (ioctl (fd, CDROM_SELECT_SPEED, cdromspeed))
			perror(" CDROM_SELECT_SPEED failed");
	}
#endif
#ifdef HDIO_DRIVE_CMD
	if (set_wcache) {
#ifdef DO_FLUSHCACHE
#ifndef WIN_FLUSHCACHE
#define WIN_FLUSHCACHE 0xe7
#endif
		unsigned char flushcache[4] = {WIN_FLUSHCACHE,0,0,0};
#endif /* DO_FLUSHCACHE */
		unsigned char args[4] = {WIN_SETFEATURES,0,0,0};
		args[2] = wcache ? 0x02 : 0x82;
		if (get_wcache) {
			printf(" setting drive write-caching to %ld", wcache);
			on_off(wcache);
		}
#ifdef HDIO_SET_WCACHE
		if (ioctl(fd, HDIO_SET_WCACHE, wcache))
#endif
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(setcache) failed");
#ifdef DO_FLUSHCACHE
		if (!wcache && ioctl(fd, HDIO_DRIVE_CMD, &flushcache))
			perror (" HDIO_DRIVE_CMD(flushcache) failed");
#endif /* DO_FLUSHCACHE */
	}
	if (set_standbynow) {
#ifndef WIN_STANDBYNOW1
#define WIN_STANDBYNOW1 0xE0
#endif
#ifndef WIN_STANDBYNOW2
#define WIN_STANDBYNOW2 0x94
#endif
		unsigned char args1[4] = {WIN_STANDBYNOW1,0,0,0};
		unsigned char args2[4] = {WIN_STANDBYNOW2,0,0,0};
		if (get_standbynow)
			printf(" issuing standby command\n");
		if (ioctl(fd, HDIO_DRIVE_CMD, &args1)
		 && ioctl(fd, HDIO_DRIVE_CMD, &args2))
			perror(" HDIO_DRIVE_CMD(standby) failed");
	}
	if (set_sleepnow) {
#ifndef WIN_SLEEPNOW1
#define WIN_SLEEPNOW1 0xE6
#endif
#ifndef WIN_SLEEPNOW2
#define WIN_SLEEPNOW2 0x99
#endif
		unsigned char args1[4] = {WIN_SLEEPNOW1,0,0,0};
		unsigned char args2[4] = {WIN_SLEEPNOW2,0,0,0};
		if (get_sleepnow)
			printf(" issuing sleep command\n");
		if (ioctl(fd, HDIO_DRIVE_CMD, &args1)
		 && ioctl(fd, HDIO_DRIVE_CMD, &args2))
			perror(" HDIO_DRIVE_CMD(sleep) failed");
	}
#endif
#if defined(WIN_SECURITY_SET_PASS) && defined(IDE_DRIVE_TASK_NO_DATA)
	if (set_security) {
#ifndef TASKFILE_OUT
#define TASKFILE_OUT 4
#endif
#ifndef IDE_DRIVE_TASK_OUT
#define IDE_DRIVE_TASK_OUT 3
#endif
#ifndef HDIO_DRIVE_TASKFILE
#define HDIO_DRIVE_TASKFILE 0x031d
#endif
		int err;
		const char *description;
		struct request_s {
			ide_task_request_t req	__attribute__((packed));
			char data[512]		__attribute__((packed));
		} request;
		memset(&request, 0, sizeof(request));
		((task_struct_t *)(&request.req.io_ports))->command = security_command;
		request.req.data_phase	= TASKFILE_OUT;
		request.req.req_cmd	= IDE_DRIVE_TASK_OUT; /* IN? */
		request.req.out_size	= 512;
		request.data[0] = (security_master & 0x01);
		memcpy(&request.data[2], security_password, 32);

		/* Not setting any out_flags causes a segfault and most
		   of the times a kernel panic */
		request.req.out_flags.b.status_command = 1;

		switch (security_command) {
			case WIN_SECURITY_UNLOCK:
				description = "SECURITY_UNLOCK";
				break;
			case WIN_SECURITY_SET_PASS:
				description = "SECURITY_SET_PASS";
				request.data[1] = (security_mode & 0x01);
				if (security_master) {
					/* master password revision code */
					request.data[34] = 0x11;
					request.data[35] = 0xff;
				}
				break;
			case WIN_SECURITY_DISABLE:
				description = "SECURITY_DISABLE";
				break;
			case WIN_SECURITY_ERASE_UNIT:
				description = "SECURITY_ERASE";
				request.data[0] |= (enhanced_erase & 0x02);
				break;
			default:
				description = "NOTHING";
		}

		printf(" Issuing %s command, password=\"%s\", user=%s",
			description, security_password, 
			request.data[0] ? "master" : "user");
		if (security_command == WIN_SECURITY_SET_PASS)
			printf(", mode=%s", request.data[1] ? "max" : "high");
		printf("\n");
/* Since the Linux kernel until at least 2.6.12 segfaults on the first command
   when issued on a locked drive the actual erase is never issued.
   One could patch the code to issue separate commands for erase prepare and
   erase to erase a locked drive.
*/
		if (security_command == WIN_SECURITY_ERASE_UNIT) {
			unsigned char args[4] = {WIN_SECURITY_ERASE_PREPARE,0,0,0};
			if (ioctl(fd, HDIO_DRIVE_CMD, &args))
				perror(" HDIO_DRIVE_CMD(erase prepare) failed");
			else
				if ((ioctl(fd, HDIO_DRIVE_TASKFILE, &request))) {
					err = errno;
					perror("Problem issuing erase command");
					if (err != 0)
						printf("Error: %d\n", err);
					if (err == EINVAL)
						printf("You need to configure your kernel with CONFIG_IDE_TASK_IOCTL.\n");
				}
/* We would like to issue these commands consecutively, but since the Linux
kernel until at least 2.6.12 segfaults on each command issued the second will
never be executed.
By disabling the below code block one is at least able to issue the commands
consecutively in two runs (assuming the segfault isn't followed by an oops.
*/
		} else if (security_command == WIN_SECURITY_DISABLE) {
			/* First attempt an unlock  */
			((task_struct_t *)(&request.req.io_ports))->command = WIN_SECURITY_UNLOCK;
			if ((ioctl(fd, HDIO_DRIVE_TASKFILE, &request))) {
				err = errno;
				perror("Problem issuing unlock command");
				if (err != 0)
					printf("Error: %d\n", err);
				if (err == EINVAL)
					printf("You need to configure your kernel with CONFIG_IDE_TASK_IOCTL.\n");
			} else {
				/* Then the security disable */
				((task_struct_t *)(&request.req.io_ports))->command = security_command;
				if ((ioctl(fd, HDIO_DRIVE_TASKFILE, &request))) {
					err = errno;
					perror("Problem issuing security disable command");
					if (err != 0)
						printf("Error: %d\n", err);
					if (err == EINVAL)
						printf("You need to configure your kernel with CONFIG_IDE_TASK_IOCTL.\n");
				}
			}
		} else if ((ioctl(fd, HDIO_DRIVE_TASKFILE, &request))) {
			err = errno;
			perror("Problem issuing security command");
			if (err != 0)
				printf("Error: %d\n", err);
			if (err == EINVAL)
				printf("You need to configure your kernel with CONFIG_IDE_TASK_IOCTL.\n");
		}
	}
#endif // defined(WIN_SECURITY_SET_PASS) && defined(IDE_DRIVE_TASK_NO_DATA)
#if defined(WIN_SECURITY_FREEZE_LOCK) && defined(HDIO_DRIVE_CMD)
	if (set_freeze) {
		unsigned char args[4] = {WIN_SECURITY_FREEZE_LOCK,0,0,0};
		printf(" issuing Security Freeze command\n");
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(freeze) failed");
	}
#endif // defined(WIN_SECURITY_FREEZE_LOCK) && defined(HDIO_DRIVE_CMD)
#ifdef HDIO_DRIVE_CMD
	if (set_seagate) {
		unsigned char args[4] = {0xfb,0,0,0};
		if (get_seagate)
			printf(" disabling Seagate auto powersaving mode\n");
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(seagatepwrsave) failed");
	}
	if (set_standby) {
		unsigned char args[4] = {WIN_SETIDLE1,(unsigned char)standby_requested,0,0};
		if (get_standby) {
			printf(" setting standby to %lu", standby_requested);
			interpret_standby(standby_requested);
		}
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(setidle1) failed");
	}
	if (get_hitachi_temp) {
		unsigned char args[4] = {0xf0,0,0x01,0}; /* "Sense Condition", vendor-specific */
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD(hitachisensecondition) failed");
		else {
			printf(" drive temperature (celsius) is:  ");
			if (args[2]==0)
				printf("under -20");
			else if (args[2]==0xFF)
				printf("over 107");
			else
				printf("%d", args[2]/2-20);
			printf("\n drive temperature in range:  %s\n", YN(!(args[1]&0x10)) );
		}
	}
#endif
	if (!flagcount)
		verbose = 1;

#ifdef HDIO_GET_MULTCOUNT
	if (verbose || get_mult || get_identity) {
		multcount = -1;
		if (ioctl(fd, HDIO_GET_MULTCOUNT, &multcount)) {
			if (get_mult)
				perror(" HDIO_GET_MULTCOUNT failed");
		} else if (verbose | get_mult) {
			printf(" multcount    = %2ld", multcount);
			on_off(multcount);
		}
	}
#endif
#ifdef HDIO_GET_32BIT
	if (verbose || get_io32bit) {
		if (0 == ioctl(fd, HDIO_GET_32BIT, &parm)) {
			printf(" IO_support   =%3ld (", parm);
			switch (parm) {
				case 0:	printf("default ");
				case 2: printf("16-bit)\n");
					break;
				case 1:	printf("32-bit)\n");
					break;
				case 3:	printf("32-bit w/sync)\n");
					break;
				case 8:	printf("Request-Queue-Bypass)\n");
					break;
				default:printf("\?\?\?)\n");
			}
		}
	}
#endif
#ifdef HDIO_GET_UNMASKINTR
	if (verbose || get_unmask) {
		if (0 == ioctl(fd, HDIO_GET_UNMASKINTR, &parm)) {
			printf(" unmaskirq    = %2ld", parm);
			on_off(parm);
		}
	}
#endif
#ifdef HDIO_GET_DMA
	if (verbose || get_dma) {
		if (0 == ioctl(fd, HDIO_GET_DMA, &parm)) {
			printf(" using_dma    = %2ld", parm);
			if (parm == 8)
				printf(" (DMA-Assisted-PIO)\n");
			else
				on_off(parm);
		}
	}
#endif
#ifdef HDIO_GET_QDMA
	if (get_dma_q) {
		if(ioctl(fd, HDIO_GET_QDMA, &parm)) {
			perror(" HDIO_GET_QDMA failed");
		} else {
			printf(" queue_depth  = %2ld", parm);
			on_off(parm);
		}
	}
#endif
#ifdef HDIO_GET_KEEPSETTINGS
	if (verbose || get_keep) {
		if (0 == ioctl(fd, HDIO_GET_KEEPSETTINGS, &parm)) {
			printf(" keepsettings = %2ld", parm);
			on_off(parm);
		}
	}
#endif /* HDIO_GET_KEEPSETTINGS */

#ifdef HDIO_SET_NOWERR
	if (get_nowerr) {
		if (ioctl(fd, HDIO_GET_NOWERR, &parm))
			perror(" HDIO_GET_NOWERR failed");
		else {
			printf(" nowerr       = %2ld", parm);
			on_off(parm);
		}
	}
#endif
#ifdef BLKROGET
	if (verbose || get_readonly) {
		if (ioctl(fd, BLKROGET, &parm))
			perror(" BLKROGET failed");
		else {
			printf(" readonly     = %2ld", parm);
			on_off(parm);
		}
	}
#endif
#ifdef BLKRAGET
	if (verbose || get_fsreadahead) {
		if (ioctl(fd, BLKRAGET, &parm))
			perror(" BLKRAGET failed");
		else {
			printf(" readahead    = %2ld", parm);
			on_off(parm);
		}
	}
#endif
#ifdef HDIO_GETGEO
	if (verbose || get_geom) {
		__ull blksize;
		static const char msg[] = " geometry     = %u/%u/%u, sectors = %lld, start = %ld\n";
// Note to self:  when compiled 32-bit (AMD,Mips64), the userspace version of this struct
// is going to be 32-bits smaller than the kernel representation.. random stack corruption!
		static struct hd_geometry g;
#ifdef HDIO_GETGEO_BIG
		static struct hd_big_geometry bg;
#endif

		if (0 == do_blkgetsize(fd, &blksize)) {
#ifdef HDIO_GETGEO_BIG
			if (!ioctl(fd, HDIO_GETGEO_BIG, &bg))
				printf(msg, bg.cylinders, bg.heads, bg.sectors, blksize, bg.start);
			else
#endif
			if (ioctl(fd, HDIO_GETGEO, &g))
				perror(" HDIO_GETGEO failed");
			else
				printf(msg, g.cylinders, g.heads, g.sectors, blksize, g.start);
		}
	}
#endif
#ifdef HDIO_DRIVE_CMD
	if (get_powermode) {
#ifndef WIN_CHECKPOWERMODE1
#define WIN_CHECKPOWERMODE1 0xE5
#endif
#ifndef WIN_CHECKPOWERMODE2
#define WIN_CHECKPOWERMODE2 0x98
#endif
		unsigned char args[4] = {WIN_CHECKPOWERMODE1,0,0,0};
		const char *state;
		if (ioctl(fd, HDIO_DRIVE_CMD, &args)
		 && (args[0] = WIN_CHECKPOWERMODE2) /* (single =) try again with 0x98 */
		 && ioctl(fd, HDIO_DRIVE_CMD, &args)) {
			if (errno != EIO || args[0] != 0 || args[1] != 0)
				state = "unknown";
			else
				state = "sleeping";
		} else {
			state = (args[2] == 255) ? "active/idle" : "standby";
		}
		printf(" drive state is:  %s\n", state);
	}
#endif
#ifdef HDIO_DRIVE_RESET
	if (perform_reset) {
		if (ioctl(fd, HDIO_DRIVE_RESET, NULL))
			perror(" HDIO_DRIVE_RESET failed");
	}
#endif /* HDIO_DRIVE_RESET */
#ifdef HDIO_TRISTATE_HWIF
	if (perform_tristate) {
		unsigned char args[4] = {0,tristate,0,0};
		if (ioctl(fd, HDIO_TRISTATE_HWIF, &args))
			perror(" HDIO_TRISTATE_HWIF failed");
	}
#endif /* HDIO_TRISTATE_HWIF */
#ifdef HDIO_GET_IDENTITY
	if (get_identity) {
		static struct hd_driveid id;

		if (!ioctl(fd, HDIO_GET_IDENTITY, &id)) {
			if (multcount != -1) {
				id.multsect = (unsigned char)multcount;
				id.multsect_valid |= 1;
			} else
				id.multsect_valid &= ~1;
			dump_identity(&id);
		} else if (errno == -ENOMSG)
			printf(" no identification info available\n");
		else
			perror(" HDIO_GET_IDENTITY failed");
	}
#endif
#ifdef HDIO_DRIVE_CMD
	if (get_IDentity) {
		__u16 *id;
		unsigned char args[4+512] = {WIN_IDENTIFY,0,0,1,}; // FIXME?
		unsigned i;
		if (ioctl(fd, HDIO_DRIVE_CMD, &args)) {
			args[0] = WIN_PIDENTIFY;
			if (ioctl(fd, HDIO_DRIVE_CMD, &args)) {
				perror(" HDIO_DRIVE_CMD(identify) failed");
				goto identify_abort;
			}
		}
		id = (__u16 *)&args[4];
		if (get_IDentity == 2) {
			for (i = 0; i < (256/8); ++i) {
				printf("%04x %04x %04x %04x %04x %04x %04x %04x\n", id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
				id += 8;
			}
		} else {
			for(i = 0; i < 0x100; ++i) {
				__le16_to_cpus(&id[i]);
			}
			identify((void *)id);
		}
identify_abort:	;
	}
#endif
#ifdef HDIO_SET_BUSSTATE
	if (set_busstate) {
		if (get_busstate) {
			printf(" setting bus state to %d", busstate);
			bus_state_value(busstate);
		}
		if (ioctl(fd, HDIO_SET_BUSSTATE, busstate))
			perror(" HDIO_SET_BUSSTATE failed");
	}
#endif
#ifdef HDIO_GET_BUSSTATE
	if (get_busstate) {
		if (ioctl(fd, HDIO_GET_BUSSTATE, &parm))
			perror(" HDIO_GET_BUSSTATE failed");
		else {
			printf(" busstate     = %2ld", parm);
			bus_state_value(parm);
		}
	}
#endif
#ifdef BLKRRPART
	if (reread_partn) {
		if (ioctl(fd, BLKRRPART, NULL)) {
			perror(" BLKRRPART failed");
		}
	}
#endif
#ifdef HDIO_DRIVE_CMD
	if (set_acoustic) {
		if (get_acoustic) {
			printf(" setting acoustic management to %d\n", acoustic);
		}
#if 0
		if (ioctl(fd, HDIO_SET_ACOUSTIC, acoustic))
			perror(" HDIO_SET_ACOUSTIC failed");
#else
{
		unsigned char args[4] = {WIN_SETFEATURES,0,0,0};
		args[1] = acoustic;
		args[2] = acoustic ? 0x42 : 0xc2;
		if (ioctl(fd, HDIO_DRIVE_CMD, &args))
			perror(" HDIO_DRIVE_CMD:ACOUSTIC failed");
}
#endif
	}
 #endif /* HDIO_SET_ACOUSTIC */
 #ifdef HDIO_GET_ACOUSTIC
	if (get_acoustic) {
		// FIXME:  use Word94 from IDENTIFY for this value
		if (ioctl(fd, HDIO_GET_ACOUSTIC, &parm)) {
			perror(" HDIO_GET_ACOUSTIC failed");
		} else {
			printf(" acoustic     = %2ld (128=quiet ... 254=fast)\n", parm);
		}
	}
 #endif /* HDIO_GET_ACOUSTIC */

	if (do_ctimings)
		time_cache (fd);
	if (do_timings)
		time_device (fd);
	if (do_flush)
		flush_buffer_cache (fd);
	close (fd);
}

void usage_error (int out)
{
	FILE *desc;
	int ret;

	if (out == 0) {
		desc = stdout;
		ret = 0;
	} else {
		desc = stderr;
		ret = 1;
	}

	fprintf(desc,"\n%s - get/set hard disk parameters - version %s\n\n", progname, VERSION);
	fprintf(desc,"Usage:  %s  [options] [device] ..\n\n", progname);
#if defined(_WIN32) || defined(__CYGWIN__)
	fprintf(desc,"Device:\n"
	" /dev/hd[a-z]    harddisk 0,1,...\n"
	" /dev/sd[a-z]    harddisk 0,1,...\n"
	" /dev/scd[0-9]   cd-rom 0,1,...\n\n");
#endif
	fprintf(desc,"Options:\n"
#ifdef BLKRASET
	" -a   get/set fs readahead\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -A   set drive read-lookahead flag (0/1)\n"
#endif
#ifdef HDIO_GET_BUSSTATE
	" -b   get/set bus state (0 == off, 1 == on, 2 == tristate)\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -B   set Advanced Power Management setting (1-255)\n"
#endif
#ifdef HDIO_SET_32BIT
	" -c   get/set IDE 32-bit IO setting\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -C   check IDE power mode status\n"
#endif
#ifdef HDIO_SET_DMA
	" -d   get/set using_dma flag\n"
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
	" --debug   enable debugging output\n"
#endif
	" --direct  use O_DIRECT to bypass page cache for timings\n"
#ifdef HDIO_DRIVE_CMD
	" -D   enable/disable drive defect management\n"
#endif
#ifdef CDROM_SELECT_SPEED
	" -E   set cd-rom drive speed\n"
#endif
	" -f   flush buffer cache for device on exit\n"
#ifdef HDIO_GETGEO
	" -g   display drive geometry\n"
#endif
	" -h   display terse usage information\n"
#ifdef HDIO_DRIVE_CMD
	" -H   read temperature from drive (Hitachi only)\n"
#endif
#ifdef HDIO_GET_IDENTITY
	" -i   display drive identification\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -I   detailed/current information directly from drive\n"
#endif
	" --Istdin  read identify data from stdin as ASCII hex\n"
#ifdef HDIO_DRIVE_CMD
	" --Istdout write identify data to stdout as ASCII hex\n"
#endif
#ifdef HDIO_GET_KEEPSETTINGS
	" -k   get/set keep_settings_over_reset flag (0/1)\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -K   set drive keep_features_over_reset flag (0/1)\n"
	" -L   set drive doorlock (0/1) (removable harddisks only)\n"
#endif
#ifdef HDIO_GET_ACOUSTIC
	" -M   get/set acoustic management (0-254, 128: quiet, 254: fast) (EXPERIMENTAL)\n"
#endif /* HDIO_GET_ACOUSTIC */
#ifdef HDIO_SET_MULTCOUNT
	" -m   get/set multiple sector count\n"
#endif
#ifdef HDIO_SET_NOWERR
	" -n   get/set ignore-write-errors flag (0/1)\n"
#endif
#ifdef HDIO_SET_PIO_MODE
	" -p   set PIO mode on IDE interface chipset (0,1,2,3,4,...)\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -P   set drive prefetch count\n"
#endif
	" -q   change next setting quietly\n"
#ifdef HDIO_GET_QDMA
	" -Q   get/set DMA tagged-queuing depth (if supported)\n"
#endif
#ifdef BLKROSET
	" -r   get/set device  readonly flag (DANGEROUS to set)\n"
#endif
#ifdef HDIO_SCAN_HWIF
	" -R   register an IDE interface (DANGEROUS)\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -s   set power-up in standby flag (0/1)\n"
	" -S   set standby (spindown) timeout\n"
#endif
	" -t   perform device read timings\n"
	" -T   perform cache read timings\n"
#ifdef HDIO_SET_UNMASKINTR
	" -u   get/set unmaskirq flag (0/1)\n"
#endif
#ifdef HDIO_UNREGISTER_HWIF
	" -U   un-register an IDE interface (DANGEROUS)\n"
#endif
	" -v   defaults; same as -"
#ifdef HDIO_SET_MULTCOUNT
	                         "m"
#endif
#ifdef HDIO_SET_32BIT
	                          "c"
#endif
#ifdef HDIO_SET_UNMASKINTR
	                           "u"
#endif
#ifdef HDIO_SET_DMA
	                            "d"
#endif
#ifdef HDIO_GET_KEEPSETTINGS
	                             "k"
#endif
#ifdef BLKROSET
	                              "r"
#endif
#ifdef BLKRASET
	                               "a"
#endif
#ifdef HDIO_GETGEO
	                                "g"
#endif
	                                  " for IDE drives\n"
	" -V   display program version and exit immediately\n"
#ifdef HDIO_DRIVE_RESET
	" -w   perform device reset (DANGEROUS)\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -W   set drive write-caching flag (0/1) (DANGEROUS)\n"
#endif
#ifdef HDIO_TRISTATE_HWIF
	" -x   tristate device for hotswap (0/1) (DANGEROUS)\n"
#endif
#ifdef HDIO_DRIVE_CMD
	" -X   set IDE xfer mode (DANGEROUS)\n"
	" -y   put IDE drive in standby mode\n"
	" -Y   put IDE drive to sleep\n"
	" -Z   disable Seagate auto-powersaving mode\n"
#endif
#ifdef BLKRRPART
	" -z   re-read partition table\n"
#endif
#if defined(WIN_SECURITY_SET_PASS) && defined(IDE_DRIVE_TASK_NO_DATA)
	" --security-help  display help for ATA security commands\n"
#elif defined(WIN_SECURITY_FREEZE_LOCK) && defined(HDIO_DRIVE_CMD)
	" --security-freeze   Freeze security settings until reset\n"
	" (remaining ATA security commands were unavailable at build time)\n"
#else
	" (ATA security commands were unavailable at build time)\n"
#endif
	"\n");
	exit(ret);
}

#if defined(WIN_SECURITY_SET_PASS) && defined(IDE_DRIVE_TASK_NO_DATA)
static void security_help (void)
{
	printf("\n"
	"ATA Security Commands:\n"
	" --security-freeze          Freeze security settings until reset\n\n"
	" The remainder of these are VERY DANGEROUS and can KILL your drive!\n"
	" Due to bugs in most Linux kernels, use of these commands may even\n"
	" trigger kernel segfaults or worse.  EXPERIMENT AT YOUR OWN RISK!\n\n"
	" --security-unlock PWD      Unlock drive, using password PWD\n"
	" --security-set-pass PWD    Lock drive, using password PWD (DANGEROUS)\n"
	"                             Use 'NULL' as the password to set empty password\n"
	"                             Drive gets locked when user password is selected\n"
	" --security-disable PWD     Disable drive locking, using password PWD\n"
	" --security-erase PWD       Erase (locked) drive using password PWD (DANGEROUS)\n"
	"                             (VERY VERY DANGEROUS -- DO NOT USE!!)\n"
	" --security-erase-enhanced PWD\n"
	"                            Enhanced-erase a (locked) drive, using password PWD\n"
	"                             (VERY VERY DANGEROUS -- DO NOT USE!!)\n"
	" --security-mode MODE       Select security level (high/maximum) (default high)\n"
	"     h   high security\n"
	"     m   maximum security\n"
	" --user-master USER         Select user/master password (default master)\n"
	"     u   user\n"
	"     m   master\n\n"
	);
}
#endif

#define GET_NUMBER(flag,num)	num = 0; \
				if (!*p && argc && isdigit(**argv)) \
					p = *argv++, --argc; \
				while (isdigit(*p)) { \
					flag = 1; \
					num = (num * 10) + (*p++ - '0'); \
				}

#define GET_STRING(flag, num) tmpstr = name; \
				tmpstr[0] = '\0'; \
				if (!*p && argc && isalnum(**argv)) \
					p = *argv++, --argc; \
				while (isalnum(*p) && (tmpstr - name) < 31) { \
					tmpstr[0] = *p++; \
					tmpstr[1] = '\0'; \
					++tmpstr; \
				} \
				num = translate_xfermode(name); \
				if (num == -1) \
					flag = 0; \
				else \
					flag = 1;

#define GET_ASCII_PASSWORD(flag, pwd) tmpstr = pwd; \
				memset(&pwd,0,sizeof(pwd)); \
				if (!*p && argc && isgraph(**argv)) \
					p = *argv++, --argc; \
				while ((tmpstr - pwd) < 32) { \
					if (!isgraph(*p)) { \
						if (*p > 0) { \
							flag = 0; \
							printf("Abort: Password contains non-printable characters!"); \
						} else flag = 1; \
						break; \
					} else {\
						sscanf(p,"%c",(unsigned char *) &tmpstr[0]); \
						p = p+1; \
						++tmpstr; \
					} \
				}

static int fromhex (unsigned char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	fprintf(stderr, "bad char: '%c' 0x%02x\n", c, c);
	exit(-1);
}

static int ishex (char c)
{
	return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static int identify_from_stdin (void)
{
	unsigned short sbuf[512];
	int err, wc = 0;

	do {
		int digit;
		char d[4];

		if (ishex(d[digit=0] = getchar())
		 && ishex(d[++digit] = getchar())
		 && ishex(d[++digit] = getchar())
		 && ishex(d[++digit] = getchar())) {
		 	sbuf[wc] = (fromhex(d[0]) << 12) | (fromhex(d[1]) << 8) | (fromhex(d[2]) << 4) | fromhex(d[3]);
			__le16_to_cpus((__u16 *)(&sbuf[wc]));
			++wc;
		} else if (d[digit] == EOF) {
			goto eof;
		} else if (wc == 0) {
			/* skip over leading lines of cruft */
			while (d[digit] != '\n') {
				if (d[digit] == EOF)
					goto eof;
				d[digit=0] = getchar();
			};
		}
	} while (wc < 256);
	putchar('\n');
	identify(sbuf);
	return 0;
eof:
	err = errno;
	fprintf(stderr, "read only %u/256 IDENTIFY words from stdin: %s\n", wc, strerror(err));
	exit(err);
}

int main(int argc, char **argv)
{
	char c, *p, *p1;
	char *tmpstr;
	char name[32];
#ifdef HDIO_GET_QDMA
	int neg;
#endif

#ifndef _WIN32
	if  ((progname = (char *) strrchr(*argv, '/')) == NULL)
#else
	if  ((progname = (char *) strrchr(*argv, '\\')) == NULL)
#endif
		progname = *argv;
	else
		progname++;
	++argv;

	if (!--argc)
		usage_error(1);
	while (argc--) {
		p = *argv++;
		if (*p == '-') {
			if (0 == strcmp(p, "--direct")) {
				open_flags |= O_DIRECT;
				++flagcount;
				continue;
			}
#if defined(_WIN32) || defined(__CYGWIN__)
			if (0 == strcmp(p, "--debug")) {
				win32_debug++;
				++flagcount;
				continue;
			}
#endif
			if (0 == strcmp(p, "--Istdin")) {
				identify_from_stdin();
				++flagcount;
				continue;
			}
#ifdef HDIO_DRIVE_CMD
			if (0 == strcmp(p, "--Istdout")) {
				get_IDentity = 2;
				++flagcount;
				continue;
			}
#endif
			if (!*++p)
				usage_error(1);
			while ((c = *p++)) {
				++flagcount;
				switch (c) {
					case 'V':
						fprintf(stdout, "%s %s\n", progname, VERSION);
						exit(0);
						break;
					case 'v':
						verbose = 1;
						break;
#ifdef HDIO_DRIVE_CMD
					case 'I':
						get_IDentity = 1;
						break;
#endif
#ifdef HDIO_GET_IDENTITY
					case 'i':
						get_identity = 1;
						break;
#endif
#ifdef HDIO_GETGEO
					case 'g':
						get_geom = 1;
						break;
#endif
					case 'f':
						do_flush = 1;
						break;
					case 'q':
						quiet = 1;
						noisy = 0;
						break;
#ifdef HDIO_SET_UNMASKINTR
					case 'u':
						get_unmask = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_unmask = 1;
							unmask = *p++ - '0';
						}
						break;
#endif
#ifdef HDIO_SET_DMA
					case 'd':
						get_dma = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p >= '0' && *p <= '9') {
							set_dma = 1;
							dma = *p++ - '0';
						}
						break;
#endif
#ifdef HDIO_SET_NOWERR
					case 'n':
						get_nowerr = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_nowerr = 1;
							nowerr = *p++ - '0';
						}
						break;
#endif
#ifdef HDIO_SET_PIO_MODE
					case 'p':
						noisy_piomode = noisy;
						noisy = 1;
						GET_STRING(set_piomode,piomode);
						break;
#endif
#ifdef BLKROGET
					case 'r':
						get_readonly = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_readonly = 1;
							readonly = *p++ - '0';
						}
						break;
#endif
#ifdef HDIO_SET_MULTCOUNT
					case 'm':
						get_mult = noisy;
						noisy = 1;
						GET_NUMBER(set_mult,mult);
						break;
#endif
#ifdef HDIO_SET_32BIT
					case 'c':
						get_io32bit = noisy;
						noisy = 1;
						GET_NUMBER(set_io32bit,io32bit);
						break;
#endif
#ifdef HDIO_DRIVE_CMD
					case 's':
						get_powerup_in_standby = noisy;
						noisy = 1;
						GET_NUMBER(set_powerup_in_standby,powerup_in_standby);
						if (!set_powerup_in_standby)
							fprintf(stderr, "-s: missing value\n");
						break;

					case 'S':
						get_standby = noisy;
						noisy = 1;
						GET_NUMBER(set_standby,standby_requested);
						if (!set_standby)
							fprintf(stderr, "-S: missing value\n");
						break;

					case 'D':
						get_defects = noisy;
						noisy = 1;
						GET_NUMBER(set_defects,defects);
						if (!set_defects)
							fprintf(stderr, "-D: missing value\n");
						break;
#endif
#ifdef CDROM_SELECT_SPEED
					case 'E':
						set_cdromspeed = 1;
						GET_NUMBER(set_cdromspeed,cdromspeed);
						if (!set_cdromspeed)
							fprintf (stderr, "-s: missing value\n");
						break;
#endif /* CDROM_SELECT_SPEED */
#ifdef HDIO_DRIVE_CMD
					case 'P':
						get_prefetch = noisy;
						noisy = 1;
						GET_NUMBER(set_prefetch,prefetch);
						if (!set_prefetch)
							fprintf(stderr, "-P: missing value\n");
						break;

					case 'X':
						get_xfermode = noisy;
						noisy = 1;
						GET_STRING(set_xfermode,xfermode_requested);
						if (!set_xfermode)
							fprintf(stderr, "-X: missing value\n");
						break;

					case 'K':
						get_dkeep = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_dkeep = 1;
							dkeep = *p++ - '0';
						} else
							fprintf(stderr, "-K: missing value (0/1)\n");
						break;

					case 'A':
						get_lookahead = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_lookahead = 1;
							lookahead = *p++ - '0';
						} else
							fprintf(stderr, "-A: missing value (0/1)\n");
						break;

					case 'L':
						get_doorlock = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_doorlock = 1;
							doorlock = *p++ - '0';
						} else
							fprintf(stderr, "-L: missing value (0/1)\n");
						break;

					case 'W':
						get_wcache = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_wcache = 1;
							wcache = *p++ - '0';
						} else
							fprintf(stderr, "-W: missing value (0/1)\n");
						break;

					case 'C':
						get_powermode = noisy;
						noisy = 1;
						break;

					case 'y':
						get_standbynow = noisy;
						noisy = 1;
						set_standbynow = 1;
						break;

					case 'Y':
						get_sleepnow = noisy;
						noisy = 1;
						set_sleepnow = 1;
						break;
#endif /* HDIO_DRIVE_CMD */
#ifdef BLKRRPART
					case 'z':
						reread_partn = 1;
						break;
#endif
#ifdef HDIO_DRIVE_CMD
					case 'Z':
						get_seagate = noisy;
						noisy = 1;
						set_seagate = 1;
						break;
					case 'H':
						get_hitachi_temp = noisy;
						noisy = 1;
						break;
#endif /* HDIO_DRIVE_CMD */
#ifdef HDIO_GET_KEEPSETTINGS
					case 'k':
						get_keep = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							set_keep = 1;
							keep = *p++ - '0';
						}
						break;
#endif /* HDIO_GET_KEEPSETTINGS */
#ifdef HDIO_UNREGISTER_HWIF
					case 'U':
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;

						if(! p) {
							fprintf(stderr, "expected hwif_nr\n");
							exit(1);
						}

						sscanf(p++, "%i", &hwif);

						unregister_hwif = 1;
						break;
#endif /* HDIO_UNREGISTER_HWIF */
#ifdef HDIO_SCAN_HWIF
					case 'R':
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;

						if(! p) {
							fprintf(stderr, "expected hwif_data\n");
							exit(1);
						}

						sscanf(p++, "%i", &hwif_data);

						if (argc && isdigit(**argv))
							p = *argv++, --argc;
						else {
							fprintf(stderr, "expected hwif_ctrl\n");
							exit(1);
						}

						sscanf(p, "%i", &hwif_ctrl);

						if (argc && isdigit(**argv))
							p = *argv++, --argc;
						else {
							fprintf(stderr, "expected hwif_irq\n");
							exit(1);
						}

						sscanf(p, "%i", &hwif_irq);

						*p = '\0';

						scan_hwif = 1;
						break;
#endif /* HDIO_SCAN_HWIF */
#ifdef HDIO_GET_QDMA
					case 'Q':
						get_dma_q = noisy;
						noisy = 1;
						neg = 0;
#ifdef HDIO_SET_QDMA
						GET_NUMBER(set_dma_q, dma_q);
						if (neg)
							dma_q = -dma_q;
#endif
						break;
#endif
#ifdef HDIO_DRIVE_RESET
					case 'w':
						perform_reset = 1;
						break;
#endif /* HDIO_DRIVE_RESET */
#ifdef HDIO_TRISTATE_HWIF
					case 'x':
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						if (*p == '0' || *p == '1') {
							perform_tristate = 1;
							tristate = *p++ - '0';
						} else
							fprintf(stderr, "-x: missing value (0/1)\n");
						break;

#endif /* HDIO_TRISTATE_HWIF */
#ifdef BLKRASET
					case 'a':
						get_fsreadahead = noisy;
						noisy = 1;
						GET_NUMBER(set_fsreadahead,fsreadahead);
						break;
#endif
#ifdef HDIO_DRIVE_CMD
					case 'B':
						get_apmmode = noisy;
						noisy = 1;
						GET_NUMBER(set_apmmode,apmmode);
						if (!set_apmmode)
							printf("-B: missing value (1-255)\n");
						break;
#endif /* HDIO_DRIVE_CMD */
					case 't':
						GET_NUMBER(do_timings,timings);
						if (!do_timings)
							timings = 3.0;
						do_timings = 1;
						do_flush = 1;
						break;
					case 'T':
						GET_NUMBER(do_ctimings,ctimings);
						if (!do_ctimings)
							ctimings = 2.0;
						do_ctimings = 1;
						do_flush = 1;
						break;
#ifdef HDIO_GET_BUSSTATE
					case 'b':
						get_busstate = noisy;
						noisy = 1;
						if (!*p && argc && isdigit(**argv))
							p = *argv++, --argc;
						switch (*p) {
							case '0':
							case '1':
							case '2':
								set_busstate = 1;
								busstate = *p++ - '0';
								break;
							default:
								break;
						}
						break;
#endif
#ifdef HDIO_GET_ACOUSTIC
					case 'M':
						get_acoustic = noisy; /* are we noisy? */
						noisy = 1;
						GET_NUMBER(set_acoustic,acoustic);
						break;
#endif /* HDIO_GET_ACOUSTIC */
					case 'h':
						usage_error(0);
						break;
#if defined(WIN_SECURITY_FREEZE_LOCK) && defined(HDIO_DRIVE_CMD)
					case '-':
						p1 = p;		//Save Switch-String
						while (isgraph(*p))
							p++;	//Move P forward
						if (0 == strcmp(p1, "security-freeze")) {
							set_freeze = 1;
						}
#if defined(WIN_SECURITY_SET_PASS) && defined(IDE_DRIVE_TASK_NO_DATA)
						else if (0 == strcmp(p1, "security-help")) {
							security_help();
							if (!argc)
								exit(0);
						} else if (0 == strcmp(p1, "security-unlock")) {
							open_flags = O_RDWR;
							set_security = 1;
							security_command = WIN_SECURITY_UNLOCK;
							GET_ASCII_PASSWORD(set_security,security_password);
						} else if (0 == strcmp(p1, "security-set-pass")) {
							open_flags=O_RDWR;
							set_security = 1;
							security_command = WIN_SECURITY_SET_PASS;
							GET_ASCII_PASSWORD(set_security,security_password);
							if (strcmp(security_password, "NULL") == 0)
								memset(&security_password, 0, sizeof(security_password));
						} else if (0 == strcmp(p1, "security-disable")) {
							open_flags = O_RDWR;
							set_security = 1;
							security_command = WIN_SECURITY_DISABLE;
							GET_ASCII_PASSWORD(set_security,security_password);
						} else if (0 == strcmp(p1, "security-erase")) {
							open_flags = O_RDWR;
							set_security = 1;
							security_command = WIN_SECURITY_ERASE_UNIT;
							GET_ASCII_PASSWORD(set_security,security_password);
						} else if (0 == strcmp(p1, "security-erase-enhanced")) {
							open_flags = O_RDWR;
							set_security = 1;
							enhanced_erase = 1;
							security_command = WIN_SECURITY_ERASE_UNIT;
							GET_ASCII_PASSWORD(set_security,security_password);
						} else if (0 == strcmp(p1, "security-mode")) {
							if (!*p && argc && isalpha(**argv))
								p = *argv++, --argc;
							if (*p == 'm') /* max */
								security_mode = 1;
							++p;
						} else if (0 == strcmp(p1, "user-master")) {
							if (!*p && argc && isalpha(**argv))
								p = *argv++, --argc;
							if (*p == 'u') /* user */
								security_master = 0;
							++p;
						}
#endif // defined(WIN_SECURITY_SET_PASS) && defined(IDE_DRIVE_TASK_NO_DATA)
						else {
							usage_error(1);
						}
						break;
#endif // defined(WIN_SECURITY_FREEZE_LOCK) && defined(HDIO_DRIVE_CMD)
					default:
						usage_error(1);
				}
			}
			if (!argc)
				usage_error(1);
		} else {
			process_dev (p);
		}
	}
	exit (0);
}
