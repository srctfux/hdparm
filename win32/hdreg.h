/* win32/hdreg.h */

#define WIN_SETIDLE1            0xE3
#define WIN_DOORLOCK            0xDE
#define WIN_DOORUNLOCK          0xDF
#define WIN_IDENTIFY            0xEC
#define WIN_PIDENTIFY           0xA1
#define WIN_SETFEATURES         0xEF
#define WIN_CHECKPOWERMODE1     0xE5
#define WIN_CHECKPOWERMODE2     0x98


#define HDIO_GETGEO             0x0301
/*
#define HDIO_GET_UNMASKINTR     0x0302
#define HDIO_GET_MULTCOUNT      0x0304
#define HDIO_GET_QDMA           0x0305
#define HDIO_GET_KEEPSETTINGS   0x0308
#define HDIO_GET_32BIT          0x0309
#define HDIO_GET_NOWERR         0x030a
#define HDIO_GET_DMA            0x030b
#define HDIO_GET_NICE           0x030c
*/
#define HDIO_GET_IDENTITY       0x030d
/*
#define HDIO_GET_WCACHE         0x030e
*/
#define HDIO_GET_ACOUSTIC       0x030f
/*
#define HDIO_GET_BUSSTATE       0x031a
#define HDIO_TRISTATE_HWIF      0x031b
*/
#define HDIO_DRIVE_RESET        0x031c
/*
#define HDIO_DRIVE_TASKFILE     0x031d
#define HDIO_DRIVE_TASK         0x031e
*/
#define HDIO_DRIVE_CMD          0x031f
/*
#define HDIO_SET_MULTCOUNT      0x0321
#define HDIO_SET_UNMASKINTR     0x0322
#define HDIO_SET_KEEPSETTINGS   0x0323
#define HDIO_SET_32BIT          0x0324
#define HDIO_SET_NOWERR         0x0325
#define HDIO_SET_DMA            0x0326
#define HDIO_SET_PIO_MODE       0x0327
#define HDIO_SCAN_HWIF          0x0328
#define HDIO_SET_NICE           0x0329
#define HDIO_UNREGISTER_HWIF    0x032a
#define HDIO_SET_WCACHE         0x032b
#define HDIO_SET_ACOUSTIC       0x032c
#define HDIO_SET_BUSSTATE       0x032d
#define HDIO_SET_QDMA           0x032e
*/
#define HDIO_GETGEO_BIG         0x0330


#ifdef HDIO_GET_BUSSTATE
enum {
	BUSSTATE_OFF = 0,
	BUSSTATE_ON,
	BUSSTATE_TRISTATE
};
#endif

#ifdef HDIO_GETGEO
struct hd_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;
	unsigned long start;
};
#endif

#ifdef HDIO_GETGEO_BIG
struct hd_big_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned int cylinders;
	unsigned long start;
};
#endif

#ifdef HDIO_GET_IDENTITY
#define __NEW_HD_DRIVE_ID
struct hd_driveid {
	unsigned short config;
	unsigned short cyls;
	unsigned short reserved2;
	unsigned short heads;
	unsigned short track_bytes;
	unsigned short sector_bytes;
	unsigned short sectors;
	unsigned short vendor0;
	unsigned short vendor1;
	unsigned short vendor2;
	unsigned char  serial_no[20];
	unsigned short buf_type;
	unsigned short buf_size;
	unsigned short ecc_bytes;
	unsigned char  fw_rev[8];
	unsigned char  model[40];
	unsigned char  max_multsect;
	unsigned char  vendor3;
	unsigned short dword_io;
	unsigned char  vendor4;
	unsigned char  capability;
	unsigned short reserved50;
	unsigned char  vendor5;
	unsigned char  tPIO;
	unsigned char  vendor6;
	unsigned char  tDMA;
	unsigned short field_valid;
	unsigned short cur_cyls;
	unsigned short cur_heads;
	unsigned short cur_sectors;
	unsigned short cur_capacity0;
	unsigned short cur_capacity1;
	unsigned char  multsect;
	unsigned char  multsect_valid;
	unsigned int   lba_capacity;
	unsigned short dma_1word;
	unsigned short dma_mword;
	unsigned short eide_pio_modes;
	unsigned short eide_dma_min;
	unsigned short eide_dma_time;
	unsigned short eide_pio;
	unsigned short eide_pio_iordy;
	unsigned short words69_70[2];
	unsigned short words71_74[4];
	unsigned short queue_depth;
	unsigned short words76_79[4];
	unsigned short major_rev_num;
	unsigned short minor_rev_num;
	unsigned short command_set_1;
	unsigned short command_set_2;
	unsigned short cfsse;
	unsigned short cfs_enable_1;
	unsigned short cfs_enable_2;
	unsigned short csf_default;
	unsigned short dma_ultra;
	unsigned short word89;
	unsigned short word90;
	unsigned short CurAPMvalues;
	unsigned short word92;
	unsigned short hw_config;
	unsigned short words94_125[32];
	unsigned short last_lun;
	unsigned short word127;
	unsigned short dlf;
	unsigned short csfo;
	unsigned short words130_155[26];
	unsigned short word156;
	unsigned short words157_159[3];
	unsigned short words160_255[95];
};

typedef char ASSERT_SIZEOF_HD_DRIVE_ID[sizeof(struct hd_driveid)==512];
#endif // HDIO_GET_IDENTITY

