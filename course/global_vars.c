#include "fat_struct.h"

int                     fd = 0;                      // file desc for checked disk
const int               boot_size = 512;             // boot size
struct FAT32_BOOT       boot;                        // boot sector struct
struct FAT32_FSInfo     fs_info;                     // file system info sector struct
struct FAT32_FS         file_sys;                    // file system struct

int                     bytes_in_sector =  0;        // bytes per sector
int                     sectors_in_cluster = 0;      // sectors per cluster

int                     options = 16;                // user options

int                     total_errors_count = 0;      //
int                     defected_files     = 0;      //