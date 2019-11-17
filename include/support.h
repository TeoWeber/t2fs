#include "apidisk.h"
#include "bitmap2.h"
#include "t2disk.h"

// constante de de inodes
#define POINTER_UNUSED (DWORD)0
#define INVALID_INODE_POINTER (iNode *)0

// constantes de arquivos
#define MAX_FILE_NAME_SIZE 255
#define MAX_OPEN_FILES 10
#define POINTER_START_POSITION 0
#define HANDLE_USED true
#define HANDLE_UNUSED false
#define INVALID_HANDLE (FILE2)-1
#define INVALID_RECORD_POINTER (Record *)0

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
    DWORD current_pointer;
} OpenFile;

boolean file_system_initialized = false;

MBR mbr;

Partition partitions[MAX_PARTITIONS];

int mounted_partition_index;

OpenFile open_files[MAX_OPEN_FILES];

boolean is_the_root_dir_open;

int root_dir_entry_current_pointer;

void initialize_file_system();

int fill_partition_structure(int partition, int sectors_per_block);

int reset_bitmaps(int partition);

void define_empty_inode_from_inode_pointer(iNode *inode_pointer);

int format_root_dir(int partition);

DWORD checksum(int partition);

boolean is_a_handle_used(FILE2 handle);

Record *get_record_pointer_from_file_given_filename(char *filename);

FILE2 get_first_unused_handle();

iNode *get_inode_pointer_given_inode_number(DWORD inode_number);

int read_n_bytes_from_file(DWORD pointer, int n, iNode inode, char *buffer);

int write_n_bytes_to_file(DWORD pointer, int n, iNode inode, char *buffer);

int strcmp (const char *s1, const char *s2);