/* win32/rawio.c - ioctl() emulation module for hdparm for Windows        */
/*               - by Christian Franke (C) 2006-7 -- freely distributable */

#define RAWIO_INTERNAL
#include "rawio.h"

#include "fs.h"
#include "hdreg.h"

#include <stdio.h>
#include <stddef.h> // offsetof()
#include <string.h>
#include <errno.h>
#include <io.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if _MSC_VER >= 1400
#define _WIN32_WINNT 0x0502
#include <winioctl.h>
#endif

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

/////////////////////////////////////////////////////////////////////////////

#ifndef IOCTL_STORAGE_RESET_DEVICE
#define IOCTL_STORAGE_RESET_DEVICE 0x2d5004
#endif


#ifndef IOCTL_DISK_GET_DRIVE_GEOMETRY
#define IOCTL_DISK_GET_DRIVE_GEOMETRY 0x070000

typedef enum _MEDIA_TYPE {
	Unknown
} MEDIA_TYPE;

typedef struct _DISK_GEOMETRY {
	LARGE_INTEGER Cylinders;
	MEDIA_TYPE MediaType;
	DWORD TracksPerCylinder;
	DWORD SectorsPerTrack;
	DWORD BytesPerSector;
} DISK_GEOMETRY;

#endif // IOCTL_DISK_GET_DRIVE_GEOMETRY

typedef char ASSERT_SIZEOF_DISK_GEOMETRY[sizeof(DISK_GEOMETRY) == 24];


#ifndef IOCTL_DISK_GET_LENGTH_INFO
#define IOCTL_DISK_GET_LENGTH_INFO 0x07405c

typedef struct _GET_LENGTH_INFORMATION {
	LARGE_INTEGER Length;
} GET_LENGTH_INFORMATION;

#endif // IOCTL_DISK_GET_LENGTH_INFO

typedef char ASSERT_SIZEOF_GET_LENGTH_INFORMATION[sizeof(GET_LENGTH_INFORMATION) == 8];


#ifndef SMART_RCV_DRIVE_DATA

typedef struct _IDEREGS {
	UCHAR  bFeaturesReg;
	UCHAR  bSectorCountReg;
	UCHAR  bSectorNumberReg;
	UCHAR  bCylLowReg;
	UCHAR  bCylHighReg;
	UCHAR  bDriveHeadReg;
	UCHAR  bCommandReg;
	UCHAR  bReserved;
} IDEREGS;

#endif // SMART_RCV_DRIVE_DATA

typedef char ASSERT_SIZEOF_IDEREGS[sizeof(IDEREGS) == 8];


#ifndef IOCTL_IDE_PASS_THROUGH
#define IOCTL_IDE_PASS_THROUGH 0x04d028
#endif

#pragma pack(1)

typedef struct _ATA_PASS_THROUGH {
	IDEREGS IdeReg;
	ULONG   DataBufferSize;
	UCHAR   DataBuffer[1];
} ATA_PASS_THROUGH;

#pragma pack()

typedef char ASSERT_SIZEOF_ATA_PASS_THROUGH[sizeof(ATA_PASS_THROUGH) == 12+1];


#ifndef IOCTL_ATA_PASS_THROUGH
#define IOCTL_ATA_PASS_THROUGH 0x04d02c

typedef struct _ATA_PASS_THROUGH_EX {
	USHORT  Length;
	USHORT  AtaFlags;
	UCHAR  PathId;
	UCHAR  TargetId;
	UCHAR  Lun;
	UCHAR  ReservedAsUchar;
	ULONG  DataTransferLength;
	ULONG  TimeOutValue;
	ULONG  ReservedAsUlong;
	ULONG/*_PTR*/ DataBufferOffset;
	UCHAR  PreviousTaskFile[8];
	UCHAR  CurrentTaskFile[8];
} ATA_PASS_THROUGH_EX;

typedef char ASSERT_SIZEOF_ATA_PASS_THROUGH_EX[sizeof(ATA_PASS_THROUGH_EX) == 40];

#define ATA_FLAGS_DRDY_REQUIRED 0x01
#define ATA_FLAGS_DATA_IN       0x02
#define ATA_FLAGS_DATA_OUT      0x04
#define ATA_FLAGS_48BIT_COMMAND 0x08

#endif //�IOCTL_ATA_PASS_THROUGH

#ifndef SMART_RCV_DRIVE_DATA
#define SMART_RCV_DRIVE_DATA 0x07c088

#pragma pack(1)

typedef struct _SENDCMDINPARAMS {
	ULONG  cBufferSize;
	IDEREGS  irDriveRegs;
	UCHAR  bDriveNumber;
	UCHAR  bReserved[3];
	ULONG  dwReserved[4];
	UCHAR  bBuffer[1];
} SENDCMDINPARAMS;

typedef struct _DRIVERSTATUS {
	UCHAR  bDriverError;
	UCHAR  bIDEError;
	UCHAR  bReserved[2];
	ULONG  dwReserved[2];
} DRIVERSTATUS;

typedef struct _SENDCMDOUTPARAMS {
	ULONG  cBufferSize;
	DRIVERSTATUS  DriverStatus;
	UCHAR  bBuffer[1];
} SENDCMDOUTPARAMS;


#pragma pack()
#endif // SMART_RCV_DRIVE_DATA

typedef char ASSERT_SIZEOF_SENDCMDINPARAMS [sizeof(SENDCMDINPARAMS)  == 32+1];
typedef char ASSERT_SIZEOF_SENDCMDOUTPARAMS[sizeof(SENDCMDOUTPARAMS) == 16+1];


/////////////////////////////////////////////////////////////////////////////

int win32_debug;

static void print_ide_regs(const IDEREGS * ri, const IDEREGS * ro)
{
	if (ri)
		printf("   In : CMD=0x%02x, FR=0x%02x, SC=0x%02x, SN=0x%02x, CL=0x%02x, CH=0x%02x, SEL=0x%02x\n",
			ri->bCommandReg, ri->bFeaturesReg, ri->bSectorCountReg, ri->bSectorNumberReg,
			ri->bCylLowReg,  ri->bCylHighReg,  ri->bDriveHeadReg);
	if (ro)
		printf("   Out: STS=0x%02x,ERR=0x%02x, SC=0x%02x, SN=0x%02x, CL=0x%02x, CH=0x%02x, SEL=0x%02x\n",
			ro->bCommandReg, ro->bFeaturesReg, ro->bSectorCountReg, ro->bSectorNumberReg,
			ro->bCylLowReg,  ro->bCylHighReg,  ro->bDriveHeadReg);
}


/////////////////////////////////////////////////////////////////////////////

static int ide_pass_through(HANDLE hdevice, IDEREGS * regs, void * data, unsigned datasize)
{ 
	unsigned int size = sizeof(ATA_PASS_THROUGH)-1 + datasize;
	ATA_PASS_THROUGH * buf = (ATA_PASS_THROUGH *)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
	DWORD num_out;
	const unsigned char magic = 0xcf;

	if (!buf) {
		errno = ENOMEM;
		return -1;
	}

	buf->IdeReg = *regs;
	buf->DataBufferSize = datasize;
	if (datasize)
		buf->DataBuffer[0] = magic;

	if (!DeviceIoControl(hdevice, IOCTL_IDE_PASS_THROUGH,
		buf, size, buf, size, &num_out, NULL)) {
		long err = GetLastError();
		if (win32_debug) {
			printf("  IOCTL_IDE_PASS_THROUGH failed, Error=%ld\n", err);
			print_ide_regs(regs, NULL);
		}
		VirtualFree(buf, 0, MEM_RELEASE);
		errno = ENOSYS;
		return -1;
	}

	if (buf->IdeReg.bCommandReg/*Status*/ & 0x01) {
		if (win32_debug) {
			printf("  IOCTL_IDE_PASS_THROUGH command failed:\n");
			print_ide_regs(regs, &buf->IdeReg);
		}
		VirtualFree(buf, 0, MEM_RELEASE);
		errno = EIO;
		return -1;
	}

	if (datasize) {
		if (!(num_out == size && buf->DataBuffer[0] != magic)) {
			if (win32_debug) {
				printf("  IOCTL_IDE_PASS_THROUGH output data missing (%lu, %lu)\n",
					num_out, buf->DataBufferSize);
				print_ide_regs(regs, &buf->IdeReg);
			}
			VirtualFree(buf, 0, MEM_RELEASE);
			errno = EIO;
			return -1;
		}
		memcpy(data, buf->DataBuffer, datasize);
	}

	if (win32_debug) {
		printf("  IOCTL_IDE_PASS_THROUGH succeeded, bytes returned: %lu (buffer %lu)\n",
			num_out, buf->DataBufferSize);
		print_ide_regs(regs, &buf->IdeReg);
	}
	*regs = buf->IdeReg;

	VirtualFree(buf, 0, MEM_RELEASE);
	return 0;
}


/////////////////////////////////////////////////////////////////////////////

static int ata_pass_through(HANDLE hdevice, IDEREGS * regs, void * data, int datasize)
{ 
	typedef struct {
		ATA_PASS_THROUGH_EX apt;
		ULONG Filler;
		UCHAR ucDataBuf[512];
	} ATA_PASS_THROUGH_EX_WITH_BUFFERS;

	ATA_PASS_THROUGH_EX_WITH_BUFFERS ab;
	IDEREGS * ctfregs;
	unsigned int size;
	DWORD num_out;
	const unsigned char magic = 0xcf;

	memset(&ab, 0, sizeof(ab));
	ab.apt.Length = sizeof(ATA_PASS_THROUGH_EX);
	//ab.apt.PathId = 0;
	//ab.apt.TargetId = 0;
	//ab.apt.Lun = 0;
	ab.apt.TimeOutValue = 10;
	size = offsetof(ATA_PASS_THROUGH_EX_WITH_BUFFERS, ucDataBuf);
	ab.apt.DataBufferOffset = size;

	if (datasize) {
		if (!(data && 0 <= datasize && datasize <= (int)sizeof(ab.ucDataBuf))) {
			errno = EINVAL;
			return -1;
		}
		ab.apt.AtaFlags = ATA_FLAGS_DATA_IN;
		ab.apt.DataTransferLength = datasize;
		size += datasize;
		ab.ucDataBuf[0] = magic;
	}
	else {
		//ab.apt.AtaFlags = 0;
		//ab.apt.DataTransferLength = 0;
	}

	ctfregs = (IDEREGS *)ab.apt.CurrentTaskFile;
	*ctfregs = *regs;

	if (!DeviceIoControl(hdevice, IOCTL_ATA_PASS_THROUGH,
		&ab, size, &ab, size, &num_out, NULL)) {
		long err = GetLastError();
		if (win32_debug) {
			printf("  IOCTL_ATA_PASS_THROUGH failed, Error=%ld\n", err);
			print_ide_regs(regs, NULL);
		}
		errno = ENOSYS;
		return -1;
	}

	if (ctfregs->bCommandReg/*Status*/ & 0x01) {
		if (win32_debug) {
			printf("  IOCTL_ATA_PASS_THROUGH command failed:\n");
			print_ide_regs(regs, ctfregs);
		}
		*regs = *ctfregs;
		errno = EIO;
		return -1;
	}

	if (datasize)
		memcpy(data, ab.ucDataBuf, datasize);

	if (win32_debug) {
		printf("  IOCTL_ATA_PASS_THROUGH succeeded, bytes returned: %lu\n", num_out);
		print_ide_regs(regs, ctfregs);
	}
	*regs = *ctfregs;

	return 0;
}


/////////////////////////////////////////////////////////////////////////////

static int smart_rcv_drive_data(HANDLE hdevice, IDEREGS * regs, void * data, unsigned datasize)
{
	SENDCMDINPARAMS inpar;
	unsigned char outbuf[sizeof(SENDCMDOUTPARAMS)-1 + 512];
	const SENDCMDOUTPARAMS * outpar;
	DWORD num_out;

	memset(&inpar, 0, sizeof(inpar));
	inpar.irDriveRegs = *regs;
	inpar.irDriveRegs.bDriveHeadReg = 0xA0;
	//inpar.bDriveNumber = 0;
	inpar.cBufferSize = 512;

	if (datasize != 512) {
		errno = EINVAL;
		return -1;
	}

	memset(&outbuf, 0, sizeof(outbuf));

	if (!DeviceIoControl(hdevice, SMART_RCV_DRIVE_DATA, &inpar, sizeof(SENDCMDINPARAMS)-1,
		outbuf, sizeof(SENDCMDOUTPARAMS)-1 + 512, &num_out, NULL)) {
		long err = GetLastError();
		if (win32_debug) {
			printf("  SMART_RCV_DRIVE_DATA failed, Error=%ld\n", err);
			print_ide_regs(regs, NULL);
		}
		errno = ENOSYS;
		return -1;
	}

	outpar = (const SENDCMDOUTPARAMS *)outbuf;

	if (outpar->DriverStatus.bDriverError || outpar->DriverStatus.bIDEError) {
		if (win32_debug) {
			printf("  SMART_RCV_DRIVE_DATA failed, DriverError=0x%02x, IDEError=0x%02x\n",
				outpar->DriverStatus.bDriverError, outpar->DriverStatus.bIDEError);
			print_ide_regs(regs, NULL);
		}
		errno = (!outpar->DriverStatus.bIDEError ? ENOSYS : EIO);
		return -1;
	}

	memcpy(data, outpar->bBuffer, 512);

	if (win32_debug) {
		printf("  SMART_RCV_DRIVE_DATA suceeded, bytes returned: %lu (buffer %lu)\n",
			num_out, outpar->cBufferSize);
		print_ide_regs(regs, NULL);
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////

static int try_ata_pass_through(HANDLE hdevice, IDEREGS * regs, void * data, int datasize)
{
	static char avail = 0x7;
	int rc;
	if (avail & 0x1) {
		rc = ata_pass_through(hdevice, regs, data, datasize);
		if (rc >= 0 || errno != ENOSYS)
			return rc;
		avail &= ~0x1;
	}
	if ((avail & 0x2) && datasize >= 0) {
		rc = ide_pass_through(hdevice, regs, data, datasize);
		if (rc >= 0 || errno != ENOSYS)
			return rc;
		avail &= ~0x2;
	}
	if ((avail & 0x4) && regs->bCommandReg == WIN_IDENTIFY) {
		rc = smart_rcv_drive_data(hdevice, regs, data, datasize);
		if (rc >= 0 || errno != ENOSYS)
			return rc;
		avail &= ~0x4;
	}
	if (win32_debug) {
		printf("  No ATA PASS THROUGH I/O control available\n");
		print_ide_regs(regs, NULL);
	}
	errno = ENOSYS;
	return -1;
}


/////////////////////////////////////////////////////////////////////////////

static int get_disk_geometry(HANDLE h, DISK_GEOMETRY * geo)
{
	DWORD num_out;
	if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, geo, sizeof(*geo), &num_out, NULL)) {
		long err = GetLastError();
		if (win32_debug)
			printf("  IOCTL_DISK_GET_DRIVE_GEOMETRY failed, Error=%ld\n", err);
		errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
		return -1;
	}
	if (win32_debug)
		printf("  IOCTL_DISK_GET_DRIVE_GEOMETRY succeeded, bytes returned: %lu\n", num_out);
	return 0;
}

static __int64 get_disk_length(HANDLE h)
{
	DWORD num_out;
	GET_LENGTH_INFORMATION li;
	if (!DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &li, sizeof(li), &num_out, NULL)) {
		long err = GetLastError();
		if (win32_debug)
			printf("  IOCTL_DISK_GET_LENGTH_INFO failed, Error=%ld\n", err);
		errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
		return -1;
	}
	if (win32_debug)
		printf("  IOCTL_DISK_GET_LENGTH_INFO succeeded, bytes returned: %lu\n", num_out);
	return li.Length.QuadPart;
}


/////////////////////////////////////////////////////////////////////////////

static int reset_device(HANDLE h)
{
	DWORD num_out;
	if (!DeviceIoControl(h, IOCTL_STORAGE_RESET_DEVICE, NULL, 0, NULL, 0, &num_out, NULL)) {
		long err = GetLastError();
		if (win32_debug)
			printf("  IOCTL_STORAGE_RESET_DEVICE failed, Error=%ld\n", err);
		errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
		return -1;
	}
	if (win32_debug)
		printf("  IOCTL_STORAGE_RESET_DEVICE succeeded\n");
	return 0;
}


/////////////////////////////////////////////////////////////////////////////

static char is_cd = 0;

int win32_open(const char * name, unsigned flags, unsigned perm)
{
	int len;
	char drv[1+1] = "";
	unsigned cdno = ~0;
	int n1 = -1;
	char path[50];
	HANDLE h;
	DWORD crflags;
	(void)perm;

	if (!strncmp("/dev/", name, 5))
		name += 5;
	len = strlen(name);
	if (sscanf(name, "%*[hs]d%1[a-z]%n", drv, &n1) == 1 && n1 == len) {
		sprintf(path, "\\\\.\\PhysicalDrive%d", drv[0] - 'a');
		is_cd = 0;
	}
	else if (sscanf(name, "scd%u%n", &cdno, (n1=-1, &n1)) == 1 && n1 == len && cdno <= 15) {
		sprintf(path, "\\\\.\\CdRom%u", cdno);
		is_cd = 1;
	}
	else {
		errno = EINVAL;
		return -1;
	}

	crflags = (flags & O_DIRECT ? FILE_FLAG_NO_BUFFERING : 0);
	if ((h = CreateFileA(path,
		GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, crflags, 0)) == INVALID_HANDLE_VALUE) {
		long err = GetLastError();
		if (win32_debug)
			printf("%s: cannot open, Error=%ld\n", path, err);
		if (err == ERROR_FILE_NOT_FOUND)
			errno = ENOENT;
		else if (err == ERROR_ACCESS_DENIED)
			errno = EACCES;
		else
			errno = EIO;
		return -1;
	}

	if (win32_debug)
		printf("%s: successfully opened\n", path);

	return (int)h;
}


int win32_close(int fd)
{
	CloseHandle((HANDLE)fd);
	return 0;
}


int win32_read(int fd, char * buf, int size)
{
	DWORD num_read;
	if (!ReadFile((HANDLE)fd, buf, size, &num_read, NULL)) {
		errno = EIO;
		return -1;
	}
	return num_read;
}


long win32_lseek(int fd, long offset, int where)
{
	DWORD pos;
	if (where != SEEK_SET) {
		errno = EINVAL;
		return -1;
	}
	pos = SetFilePointer((HANDLE)fd, offset, 0, FILE_BEGIN);
	if (pos == INVALID_SET_FILE_POINTER) {
		errno = EIO;
		return -1;
	}
	return pos;
}


static void fix_id_string(unsigned char * s, int n)
{
	int i;
	for (i = 0; i < n-1; i+=2) {
		unsigned char c = s[i]; s[i] = s[i+1]; s[i+1] = c;
	}
	for (i = n-1; i > 0 && s[i] == ' '; i--)
		s[i] = 0;
}


int win32_ioctl(int fd, int code, void * arg)
{
	int rc = 0;

	switch (code) {

#ifdef BLKGETSIZE
	  case BLKGETSIZE:
	  case BLKGETSIZE64:
		{
			__int64 size = get_disk_length((HANDLE)fd);
			if (size < 0 && errno == ENOSYS) {
				DISK_GEOMETRY dg;
				rc = get_disk_geometry((HANDLE)fd, &dg);
				if (rc)
					break;
				size = dg.Cylinders.QuadPart * dg.TracksPerCylinder * dg.SectorsPerTrack * dg.BytesPerSector;
			}
			if (code == BLKGETSIZE)
				*(unsigned *)arg = (unsigned)(size >> 9);
			else
				*(unsigned __int64 *)arg = size;
		}
		break;
#endif

#ifdef HDIO_GETGEO
	  case HDIO_GETGEO:
	  case HDIO_GETGEO_BIG:
		{
			DISK_GEOMETRY dg;
			rc = get_disk_geometry((HANDLE)fd, &dg);
			if (rc)
				break;
			if (code == HDIO_GETGEO) {
				struct hd_geometry * gp = (struct hd_geometry *)arg;
				gp->cylinders = (unsigned short)(dg.Cylinders.LowPart <= 0xffff ? dg.Cylinders.LowPart : 0xffff);
				gp->heads = (unsigned char)(dg.TracksPerCylinder <= 0xff ? dg.TracksPerCylinder : 0xff);
				gp->sectors = (unsigned char)(dg.SectorsPerTrack <= 0xff ? dg.SectorsPerTrack : 0xff);
				gp->start = 0;
			}
			else {
				struct hd_big_geometry * gp = (struct hd_big_geometry *)arg;
				gp->cylinders = dg.Cylinders.LowPart;
				gp->heads = (unsigned char)(dg.TracksPerCylinder <= 0xff ? dg.TracksPerCylinder : 0xff);
				gp->sectors = (unsigned char)(dg.SectorsPerTrack <= 0xff ? dg.SectorsPerTrack : 0xff);
				gp->start = 0;
			}
		}
		break;
#endif

#ifdef HDIO_GET_IDENTITY
	  case HDIO_GET_IDENTITY:
		if (!arg) // Flush
			break;
		{
			struct hd_driveid * id = (struct hd_driveid *)arg;
			IDEREGS regs = {0,0,0,0,0,0,0,0};
			regs.bCommandReg = (!is_cd ? WIN_IDENTIFY : WIN_PIDENTIFY);
			regs.bSectorCountReg = 1;
			rc = try_ata_pass_through((HANDLE)fd, &regs, id, 512);
			if (rc)
				break;
			fix_id_string(id->model,     sizeof(id->model));
			fix_id_string(id->fw_rev,    sizeof(id->fw_rev));
			fix_id_string(id->serial_no, sizeof(id->serial_no));
		}
		break;
#endif

#ifdef HDIO_GET_ACOUSTIC
	  case HDIO_GET_ACOUSTIC:
		if (!arg) // Flush
			break;
		{
			struct hd_driveid id;
			IDEREGS regs = {0,0,0,0,0,0,0,0};
			memset(&id, 0, sizeof(id));
			regs.bCommandReg = (!is_cd ? WIN_IDENTIFY : WIN_PIDENTIFY);
			regs.bSectorCountReg = 1;
			rc = try_ata_pass_through((HANDLE)fd, &regs, &id, 512);
			if (rc)
				break;
			*(long *)arg = (id.words94_125[0] & 0xff);
		}
		break;
#endif

#ifdef HDIO_DRIVE_RESET
	  case HDIO_DRIVE_RESET:
		rc = reset_device((HANDLE)fd);
		break;
#endif

#ifdef HDIO_DRIVE_CMD
	  case HDIO_DRIVE_CMD:
		if (!arg) // Flush
			break;
		{
			// input:
			// [0]: COMMAND
			// [1]: SECTOR NUMBER (SMART) or SECTOR COUNT (other)
			// [2]: FEATURE
			// [3]: SECTOR COUNT (transfer size)
			// output:
			// [0]: STATUS
			// [1]: ERROR
			// [2]: SECTOR COUNT
			// [3]: (undefined?)
			// [4...]: data
			unsigned char * idebuf = (unsigned char *)arg;
			IDEREGS regs = {0,0,0,0,0,0,0,0};
			regs.bCommandReg             = idebuf[0];
			regs.bFeaturesReg            = idebuf[2];
			if (idebuf[3])
				regs.bSectorCountReg = idebuf[3];
			else
				regs.bSectorCountReg = idebuf[1];
			rc = try_ata_pass_through((HANDLE)fd, &regs, idebuf+4, idebuf[3] * 512);
			idebuf[0] = regs.bCommandReg;  // STS
			idebuf[1] = regs.bFeaturesReg; // ERR
			idebuf[2] = regs.bSectorCountReg;
		}
		break;
#endif

	  default:
		errno = ENOSYS;
		rc = -1;
		break;
	}

	return rc;
}
