#ifndef _CHECK_FS_H
#define _CHECK_FS_H


#include "fat_struct.h"

/* read fat and check it's first value */
int check_fat(void* fat, struct FAT_ENTRY* fat_media, off_t offset, int size);

/* check file name for invalid characters */
int check_file_name(unsigned char* name, uint8_t isroot, uint8_t attr);

/* delete spaces and add dot before extension */
void change_file_name(char* name, uint8_t attr);

/* if user enable output print file info */
int output_file_info(struct DOS_FILE* file, unsigned char* file_name, uint8_t attr);

/* try to read 'size' bytes in 'pos' */
int fs_check(off_t pos, int size);

/* get next cluster in fat (used for loop-checking)*/
uint32_t next_cluster(uint32_t cluster);

/* test file for loops, invalid file name, bad and invalid clusters in fat and entry. */
int test_file(uint8_t isroot, struct DOS_FILE* file);

#endif