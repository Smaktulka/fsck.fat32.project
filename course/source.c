#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "fat_struct.h"
#include <math.h>
#include <byteswap.h>
#define FAT_EXTD(fs)	(((1 << fs.ent_bits)-1) & ~0xf)
#define ROUND_TO_MULTIPLE(n,m) ((n) && (m) ? (n)+(m)-1-((n)-1)%(m) : 0)

int fd = 0;
int boot_size = 512;
struct FAT32_BOOT       boot;
struct FAT32_FSInfo     fs_info;
struct FAT32_FS         file_sys;
struct DOS_FILE*       chain;

int bytes_in_sector =  0;
int sectors_in_cluster = 0;

// uint16_t bigtolil16(uint16_t num)
// {
//     return (num << 8) | (num >> 8);
// }

void boot_output() 
{
    printf("jmp_bytes: %s\n", boot.jmp_bytes);
    printf("dos_version: %s\n", boot.dos_version);
    printf("sector_size: %d     -    0x%X\n", 
        boot.sector_size, boot.sector_size);
    printf("sectors in cluster: %d\n", boot.sectors_in_cluster);
    printf("reserved sectors: %d     -     0x%X\n", 
        boot.reserved_sectors, boot.reserved_sectors);
    printf("num of fats: %d\n", boot.fats_num);
    printf("num of root dir entries: %d    -    0x%X\n", 
        boot.dir_entries_num, boot.dir_entries_num);
    printf("num of sectors; %d    -    0x%X\n", 
        boot.sectors_num, boot.sectors_num);
    printf("media descriptor: %d\n", boot.media_desc);
    printf("sectors num fat16: %d    -    0x%X\n", 
        boot.sectors_num_fat16, boot.sectors_num_fat16);
    printf("sectors in track: %d    -   0x%X\n", 
        boot.sectors_in_track, boot.sectors_in_track);
    printf("read-write headers: %d    -   0x%X\n", 
        boot.rw_heads_num, boot.rw_heads_num);
    printf("number of hidden sectors: %d     -   0x%X\n",
        boot.hidden_sectors_num, boot.hidden_sectors_num);
    printf("total sector count: %d     -   0x%X\n", 
        boot.total_sector_count, boot.total_sector_count);
    printf("Extended boot Record\n");
    printf("sectors in table: %d    -    0x%X\n", 
        boot.sectors_in_table, boot.sectors_in_table);
    printf("flags: %d    -    0x%X\n", 
        boot.flags, boot.flags);
    printf("fat version: %d    -   0x%X\n", 
        boot.fat_version, boot.fat_version);
    printf("root dir cluster: %d    -    0x%X\n", 
        boot.root_dir_cluster, boot.root_dir_cluster);
    printf("fsinfo sector number: %d    -    0x%X\n", 
        boot.fsinfo_sec_num, boot.fsinfo_sec_num);
    printf("backup boot sector number: %d    -    0x%X\n", 
        boot.backup_boot_sector, boot.backup_boot_sector);
    printf("drive number: %d\n", boot.drive_num);    
    printf("win nt flags: %d\n", boot.win_nt_flags);
    printf("signature: 0x%X\n", boot.signature);
    printf("volume id: %d     -    0x%X\n", 
        boot.volume_id, boot.volume_id);    
    printf("volume label: %s\n", boot.volume_label);
    printf("file system name: %s\n", boot.fs_name);
    printf("volume label : %d    -   0x%X\n",
        boot.boot_signature, boot.boot_signature);
    
}

// uint32_t bigtolil32(uint32_t num) 
// {
//     num = ((num << 8) & 0xFF00FF00 ) | ((num >> 8) & 0xFF00FF ); 
//     return (num << 16) | (num >> 16);
// }

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

void read_fat()
{
    int total_clusters_num = file_sys.data_clusters + 2;
    int alloc_size = (total_clusters_num * 12 + 23) / 24 * 3;
    int eff_size = (total_clusters_num * file_sys.entry_size + 7) / 8ULL;
    printf("total_cluster_num: %d\n", total_clusters_num);
    printf("alloc size: %d\n", alloc_size);
    printf("eff_size: %d\n", eff_size);
    alloc_size = eff_size;

    void* first = (unsigned int*)malloc((alloc_size + 1)* sizeof(void));
    fs_read(file_sys.fat_start, alloc_size, first);
    struct FAT_ENTRY first_media, second_media;
    get_fat(&first_media, first, 0);
    printf("entry value: %d  -  0x%X\nentry reserved: %d\n", first_media.value, first_media.value, first_media.reserved);

    int first_ok = (first_media.value & FAT_EXTD(file_sys)) == FAT_EXTD(file_sys);
    printf("first ok: %d\n", first_ok);
    file_sys.fat = (unsigned char*)malloc((alloc_size + 1) * sizeof(unsigned char)); 
    //file_sys.fat = (unsigned char*)first; 
    memcpy(file_sys.fat, (unsigned char*)first, alloc_size);
    file_sys.fat[alloc_size] = 0;
    //free(first);
    void* second = NULL;
    int second_ok;
    if (file_sys.fats_num > 1)
    {   
        second = (void*)malloc((alloc_size + 1)* sizeof(char));
        fs_read(file_sys.fat_start + file_sys.fat_size, alloc_size, second);
        get_fat(&second_media, second, 0);
        second_ok = (first_media.value & FAT_EXTD(file_sys)) == FAT_EXTD(file_sys);
        printf("entry value: %d  -  0x%X\nentry reserved: %d\n", second_media.value, second_media.value, second_media.reserved);
        printf("second ok: %d\n", second_ok);
        if (!memcmp(first, second, eff_size)) 
        {
            printf("fats differs.\n");
        }
    }
    
    file_sys.cluster_owner = malloc(total_clusters_num * sizeof(struct DOS_FILE*));

    free(second);
}


// void init_chain()
// {
//     chain = (struct DOS_FILE**)malloc(1000 * sizeof(struct DOS_FILE*));

//     for (int i = 0; i < 1000; i++)
//     {
//         chain[i] = (struct DOS_FILE*)malloc(1 * sizeof(struct DOS_FILE));
//         chain[i]->dir_ent = (struct DIR_ENT*)malloc(1 * sizeof(struct DIR_ENT));
//     }
// }

enum TEST_STATE
{
    FREE_FILE = 0,
    INVALID_FILE_NAME,
    INVALID_ENTRY,
    INVALID_ENTRY_START,
    INVALID_SIZE,
    INVALID_CLUSTER,
    LOOP
};

int fs_check(off_t pos, int size)
{
    void* test = malloc(size); 
    int check;

    lseek(fd, pos, 0);
    check = (read(fd, test, size) == size);
    free(test);

    return check;
}

uint32_t get_entry_start(struct DIR_ENT* entry) 
{
    return ((uint32_t)entry->first_cluster_low | (uint32_t)entry->first_cluster_hi << 16);
}

int check_file_name(unsigned char* name, uint8_t isroot, uint8_t attr) 
{
    const char* invalid_chars = "*?/\\|,;:+=<>[]\"";

    if (name[0] == ' ') 
    {
        return INVALID_FILE_NAME;
    }

    if (name[0] == '.' && (isroot || attr == 0x20)) 
    {
        return INVALID_FILE_NAME;
    }

    for (int i = 0; i < 11; i++) 
    {
        if (name[i] == 0x7f)
        {
            return INVALID_FILE_NAME;
        }
        
        if (strchr(invalid_chars, (int)name[i]) && attr != 0x00) 
        {
            return INVALID_FILE_NAME;
        }
    }

    int space = 0;

    for (int i = 0; i < 8; i++) 
    {
        if (name[i] == ' ') 
        {
            space = 1;
        }
        else if (space) 
        {
            return INVALID_FILE_NAME;
        }
    }

    space = 0;

    for (int i = 8; i < 11; i++) 
    {
        if (name[i] == ' ')
        {
            space = 1;
        }
        else if(space) 
        {
            return INVALID_FILE_NAME;
        }
    }

    return 0;
}

#define MSDOS_DOT ".\0"	
#define MSDOS_DOTDOT "..\0"

off_t base_cluster(uint32_t cluster) 
{
    return (cluster - 2) * file_sys.cluster_size + file_sys.data_start;
}

uint32_t next_cluster(uint32_t cluster)
{
    uint32_t value;
    struct FAT_ENTRY entry;

    get_fat(&entry, file_sys.fat, cluster);

    value = entry.value;

    return (uint16_t)entry.value >= 0xfff8 ? -1 : value;
}

void change_file_name(char* name, uint8_t attr)
{
    int i = 0;
    for (; i < 8; i++)
    {
        if (name[i] == ' ')
        {
            break;
        }
    }

    if (attr == 0x10)
    {
        name[i] = 0;
        return ;
    }

    if (i == 8)
    {
        char* dotname = (char*)malloc(13);
        dotname[12] = 0;
        memcpy(dotname, name, 8);
        dotname[8] = '.';
        memcpy(dotname + 9, name + 8, 3); 
        strcpy(name, dotname);
    }
    else 
    {
        name[i++] = '.';
        name[i++] = name[8];
        name[i++] = name[9];
        name[i++]   = name[10];
        name[i]  = 0;
    }

}

int test_file(uint8_t isroot, struct DOS_FILE* file)
{
    struct DIR_ENT* entry = file->dir_ent;

    if (entry->file_name[0] == 0xE5 || entry->attributes == 15 || entry->attributes == 8)
    {
        strcat(file->path, entry->file_name);
        printf("File attr: %d\n", entry->attributes);
        printf("Full path: %s\n", file->path);
            
        return FREE_FILE;
    }

    if(check_file_name(entry->file_name, isroot, entry->attributes) == INVALID_FILE_NAME)
    {
        strcat(file->path, entry->file_name);
        printf("File attr: %d\n", entry->attributes);
        printf("Full path: %s\n", file->path);
            
        return INVALID_FILE_NAME;
    }

    change_file_name(entry->file_name, entry->attributes);
    printf("File attr: %d\n", entry->attributes);        
    strcat(file->path, entry->file_name);
    printf("Full path: %s\n", file->path);
    
    uint32_t entry_start = get_entry_start(entry);

    if (entry->attributes == 0x10)
    {
        if (entry->file_size) 
        {
            fprintf(stderr, "directory's file size is not null.\n");
        }  

        if (entry_start == boot.root_dir_cluster && isroot) 
        {
            fprintf(stderr, "root dir is point to root\n");
        }

        if (!strcmp(entry->file_name, MSDOS_DOT) && entry_start != file->parent_cluster)
        {
            fprintf(stderr, "'.' is not pointing to parent dir.\n");
        }
        
        if (!strcmp(entry->file_name, MSDOS_DOTDOT) && entry_start != file->grandp_cluster)
        {   
            fprintf(stderr, "'..' is not pointing to grand parent dir.\n");
        }
    }

    if (entry_start >= file_sys.data_clusters + 2) 
    {
        fprintf(stderr, "pointing to invalid cluster number - %d.\n", entry_start);
        
    }

    uint32_t current = 0;
    uint32_t prev = 0;
    uint32_t clusters_num = 0;
    struct DOS_FILE* owner;

    for (current = entry_start ? entry_start : -1; current != -1; current = next_cluster(current))
    {
        struct FAT_ENTRY fat_entry;
        get_fat(&fat_entry, file_sys.fat, current);

        if (!fat_entry.value || (uint16_t)fat_entry.value == 0xfff7)
        {
            fprintf(stderr, "There is bad cluster in cluster chain.\n");
            return INVALID_CLUSTER;
        } 

        if (entry->attributes != 0x10 && entry->file_size <= clusters_num * file_sys.cluster_size)
        {
            fprintf(stderr, "file size is smaller than clusters it is using.\n");
            return INVALID_SIZE;
        }

        // check for llop
        if (owner = file_sys.cluster_owner[current])    
        {
            if (owner == file)
            {
                fprintf(stderr, "there is a loop in cluster chain.\n");
                return LOOP;
            }
        }

        off_t base = base_cluster(current);
        
        if (fs_check(base, file_sys.cluster_size))
        {   
            prev = current;
            clusters_num++;
        }
        else 
        {
            fprintf(stderr, "Cluster %ld is unreadable.\n", base);
            return INVALID_CLUSTER;
        }

        file_sys.cluster_owner[current] = owner; 
    }

    return 0;
}

int files_amount = 0;


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

            if (entry->attributes == 15)
            {
                continue;
            }

            file->dir_ent = entry;
            
            printf("\nChecking file: %s\n", entry->file_name);
            if (test_file(!cluster, file))
            {
                printf("File is defected.\n");
            }
            else if (entry->attributes == 0x10 && entry->file_name[0] != '.')
            {
                uint32_t start = get_entry_start(entry);
                read_disk(start, cluster, file->path);
            }           

            // if (entry->attributes == 0x10 || entry->attributes == 0x20)
            // {
            //     if (entry->attributes == 0x10 && entry->file_name[0] != '.')
            //     {
            //         printf("dir: %s\n", entry->file_name);
            //         uint32_t next_cluster = entry->first_cluster_hi;
            //         next_cluster <<= 16;
            //         next_cluster |= entry->first_cluster_low;  
            //         test_file(!cluster, );
            //         read_disk(next_cluster);
            //     }
            //     else 
            //     {   
            //         printf("file: %s\n", entry->file_name);
            //         test_file(!cluster);
            //     }
            // }
        }
    }

}

int init_file_sys() 
{
    int error = 0;

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
        fprintf(stderr, "No root dir...");
        error = 1;
    }

    file_sys.entries_in_sec    = boot.sector_size / 32;

    file_sys.boot_backup_start = boot.backup_boot_sector * sector_size;

    printf("\n\ndata_start: %d\n", data_start);
    printf("data clusters count: %d   -   0x%X\n", file_sys.data_clusters, file_sys.data_clusters);
    printf("data sector start: %d\n", data_sector_start);
    printf("fat_start: %d   -  %x\n", fat_start, fat_start);


    if (file_sys.boot_backup_start == 0)
    {
        printf("There is no boot backup...\n");
        return error;
    }

    struct FAT32_BOOT backup;
    fs_read(file_sys.boot_backup_start, sizeof(struct FAT32_BOOT), &backup);
    if (!memcmp(&boot, &backup, sizeof(struct FAT32_BOOT)))
    {
        printf("Boot do not equals backup...\n");
    }

    return error; 
}

int read_fsinfo()
{
    int error = 0;
    unsigned char* info_buf = (unsigned char*)malloc(513 * sizeof(unsigned char));

    fs_read(file_sys.fsinfo_start, 512, info_buf);
    info_buf[512] = 0;


    memcpy(&fs_info, info_buf, sizeof(struct FAT32_FSInfo));

    if (fs_info.lead_signature != 0x41615252)
    {
        fprintf(stderr, "Invalid lead signature...\n");
        error = 1;
    }

    if (fs_info.rev_signature != 0x61417272)
    {
        fprintf(stderr, "Invalid reversed signature...\n");
        error = 1;
    }

    if (fs_info.trail_signature != 0xAA550000)
    {
        fprintf(stderr, "Invalid trail signature...\n");
        error = 1;
    }


    printf("file system info sector.\n");
    printf("lead sig 0x%X\n", fs_info.lead_signature);
    printf("rev sig 0x%X\n", fs_info.rev_signature);
    printf("free clusters %d  --  0x%X\n", 
        fs_info.free_clusters, fs_info.free_clusters);
    printf("next cluster: %d  --  0x%X\n", 
        fs_info.next_cluster, fs_info.next_cluster);
    printf("trail sig 0x%X\n", fs_info.trail_signature);

    return error;
}

void read_boot()
{
    unsigned char* buf = (unsigned char*)malloc(513 * sizeof(unsigned char));
    read(fd, buf, boot_size);
    uint8_t error = 0;

    buf[boot_size] = 0;

    memcpy(&boot, buf, sizeof(struct FAT32_BOOT));

    if (boot.sector_size == 0)
    {
        fprintf(stderr, "Seprintf();ctor size is sero...\n");
        error = 1;
    }

    if (boot.sectors_in_cluster == 0)
    {
        fprintf(stderr, "Cluster size is sero...\n");
        error = 1;
    } 

    if (!boot.fats_num && boot.fats_num > 2)
    {
        fprintf(stderr, "Invalid count of fats\n");
        error = 1;
    }
   
    if (boot.sectors_in_table == 0)
    {
        fprintf(stderr, "Size of fat is zero.\n");
        error = 1;
    }

    boot_output();

    init_file_sys();

    read_fsinfo();
}

int main (int argc, char** argv) 
{

    if (argc == 2) 
    {
        fd = open(argv[1], O_RDONLY);

        read_boot();

        file_sys.ent_bits = 28;
        file_sys.entry_size = 32;

        read_fat();

        printf("end.\n");
        
        //init_chain();
        //chain = (struct DOS_FILE*)malloc(sizeof(struct DOS_FILE));

        char* path = (char*)malloc(2 * sizeof(char));
        path[0] = '~';
        path[1] = 0;
        read_disk(boot.root_dir_cluster, boot.root_dir_cluster, path);

        close(fd);
    }

    return 0;
}