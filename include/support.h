#include <string.h>
#include "apidisk.h"
#include "bitmap2.h"
#include "t2disk.h"

// constante de de inodes
#define INVALID_PTR (DWORD)0
#define INVALID_INODE_PTR (iNode *)0

// constantes de arquivos
#define MAX_FILE_NAME_SIZE 255
#define MAX_OPEN_FILES 10
#define PTR_START_POSITION (DWORD)0
#define HANDLE_USED true
#define HANDLE_UNUSED false
#define INVALID_HANDLE (FILE2)-1
#define INVALID_RECORD_PTR 0

// constantes de partições
#define MAX_PARTITIONS 4
#define PARTITION_FORMATTED true
#define PARTITION_UNFORMATTED false
#define NO_MOUNTED_PARTITION -1

// códigos de retorno
#define SUCCESS 0
#define ERROR -1

// tipos de arquivos
#define TYPEVAL_INVALIDO 0x00
#define TYPEVAL_REGULAR 0x01
#define TYPEVAL_LINK 0x02

typedef int FILE2;
typedef int DIR2;

typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned int DWORD;

typedef struct t2fs_superbloco SuperBlock;
typedef struct t2fs_record Record;
typedef struct t2fs_inode iNode;

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

boolean file_system_initialized = false;

MBR mbr;

Partition partitions[MAX_PARTITIONS];

int mounted_partition_index;

OpenFile open_files[MAX_OPEN_FILES];

boolean is_the_root_dir_open;

DWORD root_dir_entry_current_ptr;

iNode *root_dir_inode_ptr;

void initialize_file_system();

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

void retrieve_pointers_from_block(DWORD blocknum, DWORD* ptrs);

int read_block_from_blocknum( int ptr, int blocknum, int bytes, char* buffer );

int read_n_bytes_from_file(DWORD ptr, int n, iNode inode, char *buffer);

void write_block_to_blocknum( DWORD blocknum, char* buffer );

int initialize_new_pointer_block();

void insert_pointer_in_buffer(DWORD pointer, int starting_pos, unsigned char* buffer);

int update_inode_on_disk(int inodenum, iNode inode);

int write_new_pointer_to_block(int i, DWORD blocknum, DWORD pointer);

int write_n_bytes_to_file(DWORD ptr, int n, iNode inode, char *buffer);

DWORD get_block_of_inodes_ptr_where_inode_should_be_given_inode_number(DWORD inode_number);

DWORD get_data_block_ptr_where_data_should_be_given_data_number_of_block(DWORD data_number_of_block);

iNode *get_inode_from_in_i_th_position_of_block_of_inodes(DWORD block_of_inodes_ptr, DWORD i);

int write_inode_in_i_th_position_of_block_of_inodes(DWORD block_of_inodes_ptr, iNode inode, DWORD i);

DWORD get_data_block_ptr_from_i_th_position_of_block_of_data_block_ptrs(DWORD block_data_block_ptrs_ptr, DWORD i);

int write_data_block_ptr_in_i_th_position_of_block_of_data_block_ptrs(DWORD block_data_block_ptrs_ptr, DWORD data_block_ptr, DWORD i);