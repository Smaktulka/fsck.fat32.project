#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "fat_struct.h"
#include "source.h"
#include "fat32.h"

void print_usage()
{
    printf("Usage: ./myfsck <options> <disk_name>\n");
    printf("options:\n");
    printf("-i: show all information\n");
    printf("-s: show only reserved sectors\n");
    printf("-f: show only files\n");
}

int get_options(int argc, char** argv)
{
    int ret_opt = 0, opt = 0;
    
    while((opt = getopt(argc, argv, "isf")) != -1)
    {
        switch(opt) 
        {
            case 'i':
                ret_opt |= SHOW_INFO;
                break;
            case 's':
                ret_opt |= SHOW_SECTORS;
                break;
            case 'f':
                ret_opt |= SHOW_FILES;
                break;
            default:
                return -1;
                break; 
        }
    }

    options |= ret_opt;

    return 0;
}

int check_args(int argc, char** argv) 
{
    if (argc <= 1) 
    {
        return -1;
    }

    if (get_options(argc, argv) == -1) 
    {
        return -1;
    }

    fd = open(argv[argc - 1], O_RDONLY); 

    if (fd == -1) 
    {
        fprintf(stderr, RED "Invalid or non-existable disk name\n" RESET);
        return -1;
    }

    close(fd);

    return 0;
}

int myfsck(int argc, char** argv) 
{
    fd = open(argv[argc - 1], O_RDONLY); // open disk

    if (read_boot() > 0)    
    {
        close(fd);
        return -1;
    }

    file_sys.ent_bits = 28;  // amount of bits that are used in fat32
    file_sys.entry_size = 32; // actual entry size

    if (read_fat() == INVALID_FAT_SECTOR) 
    {
        close(fd);
        return -1;
    }

    char* path = (char*)malloc(2 * sizeof(char));
    path[0] = '~';
    path[1] = 0;

    read_disk(boot.root_dir_cluster, boot.root_dir_cluster, path); // read full disk

    printf("\nerrors count: %d\n", total_errors_count);    
    printf("defected files: %d\n", defected_files);
    
    close(fd);

    return 0;
}

int main(int argc, char** argv) 
{
    if (check_args(argc, argv) == -1) 
    {
        print_usage();
        return -1;
    }
    
    return myfsck(argc, argv);
}