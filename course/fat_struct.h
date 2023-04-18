#include<inttypes.h>

struct FAT32_BOOT 
{
    // BIOS Parameter Block

    unsigned char    jmp_bytes[3];        // jmp command to code segment (skip BPB's data). 
    unsigned char    dos_version[8];      // version of dos that being used.
    uint16_t         sector_size;         // the number of bytes per sector.
    uint8_t          sectors_in_cluster;  // number of sectors per cluster.
    uint16_t         reserved_sectors;    // number of reserved sectors.
    uint8_t          fats_num;            // number of fats.
    uint16_t         dir_entries_num;     // number of root directory entries.
    uint16_t         sectors_num;         // number of sectors in volume.
    uint8_t          media_desc;          // media descriptor type.
    uint16_t         sectors_num_fat16;   // number of sectors in fat16/12.
    uint16_t         sectors_in_track;    // sectors per track.
    uint16_t         rw_heads_num;        // read-write heads or sides in storage media.
    uint32_t         hidden_sectors_num;  // number of hidden sectors.
    uint32_t         total_sector_count;  // count of sectors.

    // Extended Boot Record

    uint32_t         sectors_in_table;    // sectors in fat.
    uint16_t         flags;               // flags.
    uint16_t         fat_version;         // high byte is the major version and the low byte is minor version.
    uint32_t         root_dir_cluster;    // the root dir cluster number.
    uint16_t         fsinfo_sec_num;      // sector number. 
    uint16_t         backup_boot_sector;  // the sector number of boot backup.
    unsigned char    reserved[12];        // reserved. When the volume is formated these bytes should be zero.
    uint8_t          drive_num;           // 0x00 - for floppy, 0x80 - for hard disk.
    uint8_t          win_nt_flags;        // reserved windows nt flags.
    uint8_t          signature;           // must be 0x28 or 0x29.
    uint32_t         volume_id;           // volume id serial number.
    unsigned char    volume_label[11];    // volume label string.
    unsigned char    fs_name[8];          // file system name is always "FAT32   ".
    unsigned char    boot_code[420];      // boot code.
    uint16_t         boot_signature;      // bootable partition signature "0xAA55"

} __attribute__ ((packed));

struct FAT32_FSInfo
{

    uint32_t         lead_signature;      // lead signature "0x41615252".
    unsigned char    reserved[480];       // reserved.
    uint32_t         free_clusters;       // free cluster count. if 0xFFFFFFFF - this count is unknown.
    uint32_t         next_cluster;        // Indicates the cluster number at which the ﬁlesystem driver should start looking for available clusters.
    unsigned char    reserved_[12];       // ved.
    uint32_t         rev_signature;       // another signature "0x61417272".
    uint32_t         trail_signature;     // 0xAA550000.

} __attribute__ ((packed));

struct DIR_ENT
{

    unsigned char    file_name[11];       // name - 8 chars and 3 chars - extension.
    uint8_t          attributes;          // READ_ONLY = 0x01 || HIDDEN = 0x02 || SYSTEM = 0x04 || VOLUME_ID = 0x08 || DIRECTORY = 0x10 || ARCHIVE = 0x20.
    uint8_t          win_reserved;        // reserved for Windows NT use.  
    uint8_t          cr_time_ds;          // tenths seconds of creation time;
    uint16_t         cr_time;             // creation time << 5 bits - hour || 6 bits - minutes || 5 bits - seconds >>.
    uint16_t         cr_date;             // creation date << 7 bits - year || 4 bits - month   || 5 bits - day     >>.
    uint16_t         last_access;         // like creation date, but shows last access to file.
    uint16_t         first_cluster_hi;    // first cluster number of entry(for directory's file).
    uint16_t         last_mod_time;       // last modification time.   
    uint16_t         last_mod_date;       // last modification date.
    uint16_t         first_cluster_low;
    uint32_t         file_size;           // the size of the file in bytes.

} __attribute__ ((packed));

struct DOS_FILE
{

    struct DIR_ENT*  dir_ent;
    char*            long_name;
    off_t            offset;
    off_t            long_name_off;
    //struct DOS_FILE* parent_dir;
    //struct DOS_FILE* next_entry;
    //struct DOS_FILE* first_entry;
    uint32_t         parent_cluster;
    uint32_t         grandp_cluster;
    char             path[32768];
};

struct FAT32_FS 
{

    off_t            fat_start;           // beginning of the fat.
    off_t            fsinfo_start;        // beginning of the file system info record.
    off_t            boot_backup_start;   // beginning of boot backup.
    off_t            data_start;          // beginning of the data.
    unsigned int     fats_num;            // number of fats.
    unsigned int     fat_size;            // fat in bytes.
    unsigned int     entry_size;          // in bits.
    unsigned int     ent_bits;            // used bits of entry.
    unsigned int     cluster_size;        // in bytes;
    uint32_t         entries_in_sec;      // 
    uint32_t         data_clusters;       // without 2 reserved.
    uint32_t         root_cluster;
    long int         free_clusters;       // amount of free clusters.
  
    unsigned char*   fat;               
    struct DOS_FILE** cluster_owner;        

};


struct FAT_ENTRY
{

    uint32_t value;
    uint32_t reserved;

};