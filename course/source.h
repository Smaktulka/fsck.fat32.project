#ifndef _SOURCE_H
#define _SOURCE_H

#define MSDOS_DOT ".\0"	
#define MSDOS_DOTDOT "..\0"
#define FAT_EXTD(fs)	(((1 << fs.ent_bits)-1) & ~0xf)

#define RED     "\x1B[31m"
#define BLU     "\x1B[34m"
#define RESET   "\x1B[0m"

extern int                     fd;                 // file desc for checked disk
extern int                     boot_size;          // boot size
extern struct FAT32_BOOT       boot;               // boot sector struct
extern struct FAT32_FSInfo     fs_info;            // file system info sector struct
extern struct FAT32_FS         file_sys;           // file system struct

extern int                     bytes_in_sector;    // bytes per sector
extern int                     sectors_in_cluster; // sectors per cluster
                    
                    
extern int                     options;            // user options
extern int                     total_errors_count; // total errors count
extern int                     defected_files;     // count of defected files


enum TEST_STATE
{
    FREE_FILE = 1,
    INVALID_BOOT_SECTOR,
    INVALID_FSINFO,
    INVALID_FAT_SECTOR,
    INVALID_FILE_NAME,
    INVALID_ENTRY,
    INVALID_ENTRY_START,
    INVALID_SIZE,
    INVALID_CLUSTER,
    LOOP
};

enum OPTIONS
{
    SHOW_SECTORS = 2,
    SHOW_FILES = 4,
    SHOW_INFO = 7
};

enum FILE_ATTR
{
    READ_ONLY = 0x01,
    HIDDEN    = 0x02,
    SYSTEM    = 0x04,
    VOLUME_ID = 0x08,
    DIR       = 0x10,
    LONG_NAME = 0x0f,
    ARCHIVE   = 0x20
};


#endif 