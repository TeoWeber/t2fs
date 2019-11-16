#include "apidisk.h"
#include "bitmap2.h"
#include "t2disk.h"

#define MAX_FILE_NAME_SIZE 255

// constantes de arquivos
#define MAX_OPEN_FILES 10
#define POINTER_START_POSITION 0
#define FILE_HANDLE_USED true
#define FILE_HANDLE_UNUSED false

#define INVALID_POINTER -1
#define INVALID_HANDLE -1

// constantes de diretórios
#define MAX_OPEN_DIRS 10
#define DIR_HANDLE_UNUSED 0

// constantes de partições
#define MAX_PARTITIONS 4
#define PARTITION_FORMATTED true
#define PARTITION_UNFORMATTED false
#define NO_MOUNTED_PARTITION -1

// códigos de retorno
#define SUCCESS 0
#define ERROR -1

#define TYPEVAL_INVALIDO 0x00
#define TYPEVAL_REGULAR 0x01
#define TYPEVAL_LINK 0x02

typedef struct t2fs_superbloco SuperBlock;
typedef struct t2fs_record FileRecord;
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
    boolean is_formatted;
    DWORD boot_sector;
    DWORD final_sector;
    DWORD size_in_sectors;
    SuperBlock super_block;
} Partition;

typedef struct t_open_file
{
    boolean handle_used;
    DWORD inode_number;
    FileRecord record;
    DWORD current_pointer;
} OpenFile;

boolean file_system_initialized = false;

MBR mbr;

Partition partitions[MAX_PARTITIONS];

int mounted_partition = NO_MOUNTED_PARTITION;

OpenFile open_files[MAX_OPEN_FILES];

DWORD open_dir_inodes[MAX_OPEN_DIRS];     // Deve continuar existindo? (único diretório é a raiz)
OpenFile open_directories[MAX_OPEN_DIRS]; // Deve continuar existindo? (único diretório é a raiz)

void initialize_file_system();

DWORD checksum(int partition);

boolean verify_file_handle(FILE2 handle);

boolean verify_dir_handle(DIR2 handle);

FILE2 retrieve_free_file_handle();

DIR2 retrieve_free_dir_handle();

int retrieve_inode(DWORD inode_number, iNode *inode);

int read_n_bytes_from_file(DWORD pointer, int n, iNode inode, char *buffer);

int write_n_bytes_to_file(DWORD pointer, int n, iNode inode, char *buffer);

int retrieve_dir_record(char *path, FileRecord *record);