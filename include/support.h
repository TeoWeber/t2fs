#ifndef __SUPPORT_H_
#define __SUPPORT_H_

#include <string.h>
#include "apidisk.h"
#include "bitmap2.h"
#include "t2disk.h"

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

void initialize_file_system();

int reset_partition_sectors(int partition);

int fill_partition_structure(int partition, int sectors_per_block);

int reset_bitmaps(int partition);

void define_empty_inode_from_inode_ptr(iNode *inode_ptr);

int format_root_dir(int partition);

DWORD checksum(int partition);

boolean is_a_handle_used(FILE2 handle);

Record *get_record_ptr_from_file_given_filename(char *filename);

Record *get_i_th_record_ptr_from_root_dir(DWORD i);

DWORD get_i_th_data_block_ptr_from_file_given_file_inode_number(DWORD i, DWORD inode_number);

FILE2 get_first_unused_handle();

iNode *get_inode_ptr_given_inode_number(DWORD inode_number);

iNode *allocate_next_free_inode_given_itself_and_get_ptr(iNode inode);

int alocate_next_free_data_block_to_file_given_file_inode(iNode inode);

void retrieve_ptrs_from_block(DWORD block_number, DWORD *ptrs);

int read_block_from_block_number(int ptr, int block_number, int bytes, char *buffer);

int read_n_bytes_from_file(DWORD ptr, int n, iNode inode, char *buffer);

void write_block_to_block_number(DWORD block_number, char *buffer);

int initialize_new_ptr_block();

void insert_ptr_in_buffer(DWORD ptr, int starting_pos, unsigned char *buffer);

int write_new_ptr_to_block(int i, DWORD block_number, DWORD ptr);

int write_n_bytes_to_file(DWORD ptr, int n, int inodenum, char *buffer);

boolean is_used_record_ptr(Record *record_ptr);

int ghost_create2(char *filename);

int update_inode_on_disk(int inode_number, iNode inode);

#endif
