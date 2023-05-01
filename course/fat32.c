#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <byteswap.h>
#include "fat32.h"
#include "source.h"
#include "fat_struct.h"
#include "check_fs.h"

void fs_read(off_t offset, int size, void* data)
{
    lseek(fd, offset, 0);
    read(fd, data, size); 
}

void get_fat(struct FAT_ENTRY* entry, void* fat, uint32_t cluster)
{
    uint32_t value = (uint32_t)((unsigned int*)fat)[cluster];
    entry->value = value & 0xfffffff;
    entry->reserved = value >> 28;
}

uint32_t get_entry_start(struct DIR_ENT* entry)
{
    return ((uint32_t)entry->first_cluster_low | (uint32_t)entry->first_cluster_hi << 16);
}

off_t base_cluster(uint32_t cluster) 
{
    return (cluster - 2) * file_sys.cluster_size + file_sys.data_start;
}

int read_boot()
{
    unsigned char* buf = (unsigned char*)malloc(513 * sizeof(unsigned char));
    fs_read(0, boot_size, buf);
    uint8_t error = 0;

    buf[boot_size] = 0;

    memcpy(&boot, buf, sizeof(struct FAT32_BOOT));

    if(options & SHOW_SECTORS)
    {
        pretty_output();
    }

    if (boot.sector_size == 0)
    {
        fprintf(stderr, RED "Sector size is sero...\n");
        error++;
    }

    if (boot.sectors_in_cluster == 0)
    {
        fprintf(stderr, RED "Cluster size is sero...\n");
        error++;
    } 

    if (!boot.fats_num && boot.fats_num > 2)
    {
        fprintf(stderr, RED "Invalid count of fats\n");
        error++;
    }
   
    if (boot.sectors_in_table == 0)
    {
        fprintf(stderr, RED "Size of fat is zero.\n");
        error++;
    }

    if(total_errors_count = error)
    {
        fprintf(stderr, RED "Invalid boot sector\n" RESET);
        return INVALID_BOOT_SECTOR;
    }

    if (init_file_sys() == INVALID_BOOT_SECTOR) 
    {
        fprintf(stderr, RED "Invalid boot sector\n" RESET);
        return INVALID_BOOT_SECTOR;        
    }
    
    if (read_fsinfo() == INVALID_FSINFO) 
    {
        fprintf(stderr, RED "Invalid fsinfo sector\n" RESET);
        return INVALID_FSINFO;
    }


    if (options & SHOW_INFO)
    {
        printf("\n\ndata start: %ld\n", file_sys.data_start);
        printf("data clusters count: %d   -   0x%X\n", file_sys.data_clusters, file_sys.data_clusters);
        printf("fat_start: %ld\n", file_sys.fat_start);
    }

    return 0;
}


int init_file_sys()
{
    int retval = 0;

    int sector_size            = boot.sector_size;
    int fat_start              = sector_size * boot.reserved_sectors;
    int fat_size               = boot.sectors_in_table * sector_size;
    int data_start             = fat_start +  boot.fats_num * fat_size;
    int data_sector_start      = boot.reserved_sectors + boot.fats_num * boot.sectors_in_table;
    int data_size              = boot.total_sector_count * sector_size - data_start;
    
    file_sys.fat_start         = fat_start;
    file_sys.fat_size          = fat_size;
    file_sys.fats_num          = boot.fats_num;
    
    file_sys.cluster_size      = boot.sectors_in_cluster * sector_size;

    file_sys.data_start        = data_start;
    file_sys.data_clusters     = data_size / file_sys.cluster_size;

    file_sys.fsinfo_start      = boot.fsinfo_sec_num * sector_size;
    

    file_sys.root_cluster      = boot.root_dir_cluster;

    if (file_sys.root_cluster == 0)
    {
        fprintf(stderr, RED "No root dir...\n" RESET);
        total_errors_count++;
        retval = INVALID_BOOT_SECTOR;
    }

    file_sys.entries_in_sec    = boot.sector_size / 32;

    file_sys.boot_backup_start = boot.backup_boot_sector * sector_size;


    if (file_sys.boot_backup_start == 0)
    {
        printf(BLU "There is no boot backup...\n\n" RESET);
        return retval;
    }

    struct FAT32_BOOT backup;
    fs_read(file_sys.boot_backup_start, sizeof(struct FAT32_BOOT), &backup);

    if (!memcmp(&boot, &backup, sizeof(struct FAT32_BOOT)))
    {
        printf(BLU "Boot not equals backup...\n\n" RESET);
    }

    return retval; 
}

int read_fsinfo()
{
    int error = 0;
    unsigned char* info_buf = (unsigned char*)malloc(513 * sizeof(unsigned char));

    fs_read(file_sys.fsinfo_start, 512, info_buf);
    info_buf[512] = 0;


    memcpy(&fs_info, info_buf, sizeof(struct FAT32_FSInfo));

    if (options & SHOW_SECTORS)
    {
        printf("\n\n\t  ||____FS INFO SECTOR____||\n\n");
        printf("\tFIELD         | |\tVALUE\n");
        printf("LEAD SIGNATURE         |  0x%X\n", fs_info.lead_signature);
        printf("REVERSED SIGNATURE     |  0x%X\n", fs_info.rev_signature);
        printf("FREE CLUSTERS COUNT    |  %d\n", fs_info.free_clusters);
        printf("NEXT CLUSTER NUMBER    |  %d\n", fs_info.next_cluster);
        printf("TRAIL SIGNATURE        |  0x%X\n", fs_info.trail_signature);
    } 

    if (fs_info.lead_signature != 0x41615252)
    {
        fprintf(stderr, RED "Invalid lead signature: 0x%X\n\n" RESET, fs_info.lead_signature);
        error++;
    }

    if (fs_info.rev_signature != 0x61417272)
    {
        fprintf(stderr, RED "Invalid reversed signature: 0x%X\n\n" RESET, fs_info.rev_signature);
        error++;
    }

    if (fs_info.trail_signature != 0xAA550000)
    {
        fprintf(stderr, RED "Invalid trail signature: 0x%X\n\n" RESET, fs_info.trail_signature);
        error++;
    }

    total_errors_count += error;

    return error == 3 ? INVALID_FSINFO : 0;
}

int read_fat()
{
    int total_clusters_num = file_sys.data_clusters + 2;
    int eff_size           = (total_clusters_num * file_sys.entry_size + 7) / 8ULL;
    void* first            = (unsigned int*)malloc((eff_size + 1)* sizeof(void));
    void* second           = NULL;
    int first_check        = 0;
    int second_check       = 0;
    struct FAT_ENTRY first_media, second_media;
    
    if (first_check = check_fat(first, &first_media, file_sys.fat_start, eff_size)) 
    { 
        fprintf(stderr, RED "first fat is invalid\n\n" RESET);
    }

    if (file_sys.fats_num > 1)
    {   
        second = (void*)malloc((eff_size + 1)* sizeof(char));
        if (second_check = check_fat(second, &second_media, file_sys.fat_start + file_sys.fat_size, eff_size))
        {
            fprintf(stderr, RED "second fat is invalid\n\n" RESET);
        }
        
        if (!memcmp(first, second, eff_size)) 
        {
            printf(BLU "fats differs.\n\n" RESET);
        }

        if(first_check && !second_check) 
        {
            free(first);
            first = second;
        }
        else if (first_check && second_check)
        {
            free(first);
            free(second);
            fprintf(stderr, RED "Invalid fats.\n\n" RESET);
            total_errors_count++;
            return INVALID_FAT_SECTOR;
        }
    }

    file_sys.fat = (unsigned char*)malloc((eff_size + 1) * sizeof(unsigned char)); 
    memcpy(file_sys.fat, (unsigned char*)first, eff_size);
    file_sys.fat[eff_size] = 0;
    
    file_sys.cluster_owner = malloc(total_clusters_num * sizeof(struct DOS_FILE*));
    memset(file_sys.cluster_owner, 0, (total_clusters_num * sizeof(struct DOS_FILE*)));

    free(second);
    return 0;
}

void read_disk(uint32_t cluster, uint32_t grandp, char* file_path)
{
    int sector_num          = 0;
    int entry_num           = 0;
    struct DIR_ENT* entry   = (struct DIR_ENT*)malloc(1 * sizeof(struct DIR_ENT)); 
    off_t offset            = base_cluster(cluster);
    struct DOS_FILE* file   = (struct DOS_FILE*)malloc(sizeof(struct DOS_FILE));
    file->dir_ent           = (struct DIR_ENT*)malloc(sizeof(struct DIR_ENT));
    file->parent_cluster    = cluster;
    file->grandp_cluster    = grandp;
    strcat(file_path, "/");
    for (; sector_num < boot.sectors_in_cluster; sector_num++)
    {
        for (; entry_num < file_sys.entries_in_sec; entry_num++)
        {
            char path[32768];
            strcpy(path, file_path);
            strcpy(file->path, path);

            fs_read(offset + (entry_num * 32), sizeof(struct DIR_ENT), entry);
            entry->file_name[12] = 0;

            if (entry->file_name[0] == 0)
            {
                break;
            }

            if (entry->attributes == LONG_NAME)
            {
                continue;
            }

            file->dir_ent = entry;
            
            if (test_file(!cluster, file))
            {
                defected_files++;
                fprintf(stderr, RED "File '%s' is defected.\n\n" RESET, file->path);
            }
            else if (entry->attributes & DIR && entry->file_name[0] != '.')
            {
                uint32_t start = get_entry_start(entry);
                read_disk(start, cluster, file->path);
            }
        }
    }
}

void boot_output() 
{
    boot.jmp_bytes[3] = 0;
    printf("jmp_bytes:%s\n", boot.jmp_bytes);
    boot.jmp_bytes[3] = 'M';
    printf("dos_version:\t\t\t %s\n", boot.dos_version);
    printf("sector_size:\t\t\t %d     -    0x%X\n", 
        boot.sector_size, boot.sector_size);
    printf("sectors in cluster:\t\t\t %d\n", boot.sectors_in_cluster);
    printf("reserved sectors:\t\t\t %d     -     0x%X\n", 
        boot.reserved_sectors, boot.reserved_sectors);
    printf("num of fats:\t\t\t %d\n", boot.fats_num);
    printf("num of root dir entries:\t\t\t %d    -    0x%X\n", 
        boot.dir_entries_num, boot.dir_entries_num);
    printf("num of sectors:\t\t\t %d    -    0x%X\n", 
        boot.sectors_num, boot.sectors_num);
    printf("media descriptor:\t\t\t %d\n", boot.media_desc);
    printf("sectors num fat16:\t\t\t %d    -    0x%X\n", 
        boot.sectors_num_fat16, boot.sectors_num_fat16);
    printf("sectors in track:\t\t\t %d    -   0x%X\n", 
        boot.sectors_in_track, boot.sectors_in_track);
    printf("read-write headers:\t\t\t %d    -   0x%X\n", 
        boot.rw_heads_num, boot.rw_heads_num);
    printf("number of hidden sectors:\t\t\t %d     -   0x%X\n",
        boot.hidden_sectors_num, boot.hidden_sectors_num);
    printf("total sector count:\t\t\t %d     -   0x%X\n", 
        boot.total_sector_count, boot.total_sector_count);
    printf("||_____Extended boot Record______||\n");
    printf("sectors in table:\t\t\t %d    -    0x%X\n", 
        boot.sectors_in_table, boot.sectors_in_table);
    printf("flags:\t\t\t %d    -    0x%X\n", 
        boot.flags, boot.flags);
    printf("fat version:\t\t\t %d    -   0x%X\n", 
        boot.fat_version, boot.fat_version);
    printf("root dir cluster:\t\t\t %d    -    0x%X\n", 
        boot.root_dir_cluster, boot.root_dir_cluster);
    printf("fsinfo sector number:\t\t\t %d    -    0x%X\n", 
        boot.fsinfo_sec_num, boot.fsinfo_sec_num);
    printf("backup boot sector number:\t\t\t %d    -    0x%X\n", 
        boot.backup_boot_sector, boot.backup_boot_sector);
    printf("drive number:\t\t\t %d\n", boot.drive_num);    
    printf("win nt flags:\t\t\t %d\n", boot.win_nt_flags);
    printf("signature:\t\t\t 0x%X\n", boot.signature);
    printf("volume id:\t\t\t %d     -    0x%X\n", 
        boot.volume_id, boot.volume_id);    
    printf("volume label:\t\t\t %s\n", boot.volume_label);
    printf("file system name:\t\t\t %s\n", boot.fs_name);
    printf("volume label:\t\t\t %d    -   0x%X\n",
        boot.boot_signature, boot.boot_signature);
    
}

void pretty_output() 
{
    printf("\t     ||____BOOT RECORD___||\n\n");
    printf("\tFIELD         | |\tVALUE\n");
    boot.jmp_bytes[3] = 0;
    printf("JUMP BYTES             |  %s\n", boot.jmp_bytes);
    boot.jmp_bytes[3] = 'M';
    printf("DOS VERSION            |  %s\n", boot.dos_version);
    printf("SECTOR SIZE            |  %d\n", boot.sector_size);
    printf("SECTORS PER CLUSTER    |  %d\n", boot.sectors_in_cluster);
    printf("RESERVED SECTORS       |  %d\n", boot.reserved_sectors);
    printf("FAT COUNT              |  %d\n", boot.fats_num);
    printf("ROOT DIR ENTR. COUNT   |  %d\n", boot.dir_entries_num);
    printf("SECTORS AMOUNT         |  %d\n", boot.sectors_num);
    printf("MEDIA DESC             |  %d\n", boot.media_desc);
    printf("SECTORS COUNT(fat16/12)|  %d\n", boot.sectors_num_fat16);
    printf("SECTORS IN TRACK       |  %d\n", boot.sectors_in_track);
    printf("READ-WRITE HEADS       |  %d\n", boot.rw_heads_num);
    printf("HIDDEN SECTORS COUNT   |  %d\n", boot.hidden_sectors_num);
    printf("TOTAL SECTOR COUNT     |  %d\n", boot.total_sector_count);
    
    printf("\n\t||____EXTENDED BOOT RECORD____||\n\n");
    printf("\tFIELD         | |\tVALUE\n");
    printf("SECTORS PER TABLE      |  %d\n", boot.sectors_in_table);
    printf("FLAGS                  |  %d\n", boot.flags);
    printf("FAT VERSION            |  %d\n", boot.fat_version);
    printf("ROOT DIR CLUSTER       |  %d\n", boot.root_dir_cluster);
    printf("FSINFO SECTOR NUMBER   |  %d\n", boot.fsinfo_sec_num);
    printf("BOOT BACKUP SECTOR NUM |  %d\n", boot.backup_boot_sector);
    printf("DRIVE NUMBER           |  %d\n", boot.drive_num);    
    printf("WIN NT FLAGS           |  %d\n", boot.win_nt_flags);
    printf("SIGNATURE              |  %d\n", boot.signature);
    printf("VOLUME ID              |  %d\n", boot.volume_id);    
    boot.volume_label[11] = 0;
    printf("VOLUME LABEL           |  %s\n", boot.volume_label);
    boot.volume_label[11] = 'F';
    boot.fs_name[8] = 0;
    printf("FILE SYSTEM NAME       |  %s\n", boot.fs_name);
    printf("BOOT SIGNATURE         |  0x%X\n", boot.boot_signature);
    
}


