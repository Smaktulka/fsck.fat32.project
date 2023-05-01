#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "check_fs.h"
#include "source.h"
#include "fat32.h"


int check_fat(void* fat, struct FAT_ENTRY* fat_media, off_t offset, int size)
{
    fs_read(offset, size, fat);
    get_fat(fat_media, fat, 0);

    return (fat_media->value & FAT_EXTD(file_sys)) != FAT_EXTD(file_sys);
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
            space++;
        }
        else if (space) 
        {
            return INVALID_FILE_NAME;
        }
    }

    if (space == 8)
    {
        return INVALID_FILE_NAME;
    }

    space = 0;

    for (int i = 8; i < 11; i++) 
    {
        if (name[i] == ' ')
        {
            space++;
        }
        else if(space) 
        {
            return INVALID_FILE_NAME;
        }
    }

    return 0;
}


void change_file_name(char* name, uint8_t attr)
{
    int i = 0;
    char* ext = name + 8;
    int ext_space = 0;
    for (; i < 8; i++)
    {
        if (name[i] == ' ')
        {
            break;
        }
    }

    if (attr & DIR)
    {
        name[i] = 0;
        return;
    }

    for (int j = 0; j < 3; j++)
    {
        if (ext[j] == ' ')
        {
            ext_space++;
        }
    }

    if (attr & ARCHIVE && ext_space == 3)
    {
        return;
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
        name[i++] = name[10];
        name[i]   = 0;
    }

}

int output_file_info(struct DOS_FILE* file, unsigned char* file_name, uint8_t attr)
{
    if (options & SHOW_FILES)  
    {
        printf("\nChecking file: %s\n", file_name);
        strcat(file->path, file_name);
        printf("File attr: %d\n", attr);
        printf("Full path: %s\n", file->path);
        
        return 1;
    }
    else 
    {
        strcat(file->path, file_name);
    }

    return 0;
}


int fs_check(off_t pos, int size)
{
    void* test = malloc(size); 
    int check;

    lseek(fd, pos, 0);
    check = (read(fd, test, size) == size);
    free(test);

    return check;
}

uint32_t next_cluster(uint32_t cluster)
{
    uint32_t value;
    struct FAT_ENTRY entry;

    get_fat(&entry, file_sys.fat, cluster);

    value = entry.value;

    return (uint16_t)entry.value >= 0xfff8 ? -1 : value;
}


int test_file(uint8_t isroot, struct DOS_FILE* file)
{
    struct DIR_ENT* entry    = file->dir_ent;
    uint8_t attr             = entry->attributes;
    unsigned char* file_name = (unsigned char*)malloc(12 * sizeof(unsigned char));
    strcpy(file_name, entry->file_name);
    file_name[11] = 0;

    if (file_name[0] == 0xE5 || attr == VOLUME_ID)
    {
        if (options & SHOW_FILES)
        {
            printf("\nChecking file: %s\n", file_name);
            printf("File attr: %d\n", attr);
        }

        return FREE_FILE;
    }
    
    if(check_file_name(file_name, isroot, attr) & INVALID_FILE_NAME)
    {
        output_file_info(file, file_name, attr);

        fprintf(stderr, RED "Invalid file name...\n" RESET);
        total_errors_count++;

        return INVALID_FILE_NAME;
    }
    
    change_file_name(file_name, attr);
    entry->attributes = attr;
    output_file_info(file, file_name, attr);


    uint32_t entry_start = get_entry_start(entry);

    if (attr & DIR)
    {
        int check_count = total_errors_count;

        if (entry->file_size) 
        {
            fprintf(stderr, RED "Directory's file size is not null.\n" RESET);
            total_errors_count++;
        }  

        if (entry_start == boot.root_dir_cluster && isroot) 
        {
            fprintf(stderr, RED "Root dir is point to root\n" RESET);
            total_errors_count++;    
        }

        if (!strcmp(file_name, MSDOS_DOT) && entry_start != file->parent_cluster)
        {
            fprintf(stderr, RED "'.' is not pointing to parent dir.\n" RESET);
            total_errors_count++;
        }
        
        if (!strcmp(file_name, MSDOS_DOTDOT) && entry_start != file->grandp_cluster)
        {   
            fprintf(stderr, RED "'..' is not pointing to grand parent dir.\n" RESET);
            total_errors_count++;
        }

        if (total_errors_count > check_count)
        {
            return INVALID_ENTRY;
        }
    }

    if (entry_start >= file_sys.data_clusters + 2) 
    {
        fprintf(stderr, RED "Pointing to invalid cluster number - %d.\n", entry_start);
        total_errors_count++;

        return INVALID_ENTRY;
    }

    uint32_t current = 0;
    uint32_t prev = 0;
    uint32_t clusters_num = 0;
    struct DOS_FILE* owner;

    if (entry->attributes & DIR) 
    {
        return 0;
    }

    for (current = entry_start ? entry_start : -1; current != -1;)
    {
        struct FAT_ENTRY fat_entry;
        get_fat(&fat_entry, file_sys.fat, current);

        if (!fat_entry.value || (uint16_t)fat_entry.value == 0xfff7)
        {
            fprintf(stderr, RED "There is bad cluster in cluster chain.\n" RESET);
            total_errors_count++;
            return INVALID_CLUSTER;
        } 

        if (!(entry->attributes & DIR) && entry->file_size <= clusters_num * file_sys.cluster_size)
        {
            fprintf(stderr, RED "file size is smaller than clusters it is using.\n" RESET);
            total_errors_count++;
            return INVALID_SIZE;
        }

        // check for loop
        if (owner = file_sys.cluster_owner[current])    
        {
            if (owner == file)
            {
                fprintf(stderr, RED "There is a loop in cluster chain.\n" RESET);
                total_errors_count++;
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
            fprintf(stderr, RED "Cluster %ld is unreadable.\n" RESET, base);
            total_errors_count++;
            return INVALID_CLUSTER;
        }

        file_sys.cluster_owner[current] = file; 
        current = next_cluster(current);
    }

    return 0;
}