#ifndef __SUPPORT_H_
#define __SUPPORT_H_

#include <string.h>
#include <stdlib.h>
#include "apidisk.h"
#include "bitmap2.h"
#include "t2disk.h"
#include "t2fs.h"

typedef struct t_mbr
{
    WORD version;
    WORD sectorSize;
    WORD InitialByteOfPartitionTable;
    WORD numberOfPartitions;

    DWORD partition0BootSector;
    DWORD partition0FinalSector;
    BYTE partition0Name[24];

    DWORD partition1BootSector;
    DWORD partition1FinalSector;
    BYTE partition1Name[24];

    DWORD partition2BootSector;
    DWORD partition2FinalSector;
    BYTE partition2Name[24];

    DWORD partition3BootSector;
    DWORD partition3FinalSector;
    BYTE partition3Name[24];

} MBR;

typedef struct t_partition
{
    DWORD boot_sector;
    DWORD final_sector;
    DWORD size_in_sectors;
    boolean is_formatted;
    SuperBlock super_block;
    DWORD number_of_inodes;
    DWORD number_of_data_blocks;
} Partition;

typedef struct t_open_file
{
    boolean handle_used;
    Record record;
    DWORD current_ptr;
} OpenFile;

extern boolean file_system_initialized;

extern MBR mbr;

extern Partition partitions[MAX_PARTITIONS];
extern int mounted_partition_index;

extern boolean is_the_root_dir_open;
extern DWORD root_dir_entry_current_ptr;
extern iNode *root_dir_inode_ptr;

extern OpenFile open_files[MAX_OPEN_FILES];

void initialize_file_system();

int reset_partition_sectors(int partition);

int fill_partition_structure(int partition, int sectors_per_block);

int reset_bitmaps(int partition);

void define_empty_inode_from_inode_ptr(iNode *inode_ptr);

int format_root_dir(int partition);

DWORD checksum(int partition);

boolean is_a_handle_used(FILE2 handle);

Record *get_record_ptr_from_file_given_filename(char *filename);

DWORD get_i_from_first_invalid_record();

DWORD get_free_inode_number_in_partition();

DWORD get_free_data_block_number_in_partition();

Record *get_i_th_record_ptr_from_root_dir(DWORD i);

int set_i_th_record_ptr_on_root_dir_given_itself(DWORD i, Record *record_ptr);

DWORD get_i_th_data_block_ptr_from_file_given_file_inode_number(DWORD i, DWORD inode_number);

FILE2 get_first_unused_handle();

iNode *get_inode_ptr_given_inode_number(DWORD inode_number);

int write_data_block_ptr_to_block_of_data_block_ptrs_given_its_ptr(int i, DWORD block_of_data_block_ptrs_ptr, DWORD ptr);

void get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(DWORD block_of_data_block_ptrs_ptr, DWORD *array_of_data_block_ptrs);

int write_block_of_data_to_data_block_given_its_ptr(DWORD data_block_ptr, char *buffer);

int read_block_from_data_block_given_its_ptr(int ptr, int data_block_ptr, int bytes, char *buffer);

int read_n_bytes_from_file_given_its_inode(DWORD ptr, int n, iNode inode, char *buffer);

int initialize_new_block_of_data_block_ptrs_and_get_its_number();

void insert_DWORD_value_in_its_position_on_buffer(DWORD dword_value, int starting_pos, unsigned char *buffer);

int write_n_bytes_to_file_given_its_inode_number(DWORD ptr, int n, int inode_number, char *buffer);

boolean is_used_record_ptr(Record *record_ptr);

int ghost_create2(char *filename);

int update_inode_on_disk(int inode_number, iNode inode);

DWORD get_data_block_ptr_given_data_block_number(DWORD data_block_number);

DWORD get_data_block_number_given_data_block_ptr(DWORD data_block_ptr);

#endif
