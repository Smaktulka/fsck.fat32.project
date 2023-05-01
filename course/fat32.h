#ifndef _FAT32_H
#define _FAT32_H

#include "fat_struct.h"

/* read blocks of fat32 file system */
void fs_read(off_t offset, int size, void* data);

/* get entry start cluster */
uint32_t get_entry_start(struct DIR_ENT* entry);

/* get offset in bytes, where 'cluster' points at */
off_t base_cluster(uint32_t cluster);

/* get entry from fat */
void get_fat(struct FAT_ENTRY* entry, void* fat, uint32_t cluster);

/* read boot sector and check for errors */
int read_boot();

/* output boot sector */
void boot_output();

/* init file_sys and check for errors */
int init_file_sys();

/* read fsinfo sector */
int read_fsinfo();

/* read file allocation table sector */
int read_fat();

/* scan disk for errors */
void read_disk(uint32_t cluster, uint32_t grandp, char* file_path);

/**/
void pretty_output(); 

#endif




