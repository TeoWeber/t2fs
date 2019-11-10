#include <stdio.h>
#include <stdlib.h>
#include "apidisk.h"
#include "bitmap2.h"
#include "t2disk.h"

#define MAX_FILE_NAME_SIZE 255

// constantes de arquivos
#define MAX_OPEN_FILES 10
#define FILE_HANDLE_UNUSED 0
#define POINTER_START_POSITION 0

#define INVALID_POINTER -1
#define INVALID_HANDLE -1

// constantes de diretórios
#define MAX_OPEN_DIRS 10
#define DIR_HANDLE_UNUSED 0

// constantes de partições
#define MAX_PARTITIONS 4
#define PARTITION_FORMATTED true
#define PARTITION_UNFORMATTED false
#define UNDEFINED_BOOT_SECTOR 0
#define NO_MOUNTED_PARTITION -1

// códigos de retorno
#define SUCCESS 0
#define ERROR -1

#define	TYPEVAL_INVALIDO	0x00
#define	TYPEVAL_REGULAR		0x01
#define	TYPEVAL_LINK		0x02

boolean is_partition_formatted[MAX_PARTITIONS] = { PARTITION_UNFORMATTED };
DWORD partition_boot_sectors[MAX_PARTITIONS] = { UNDEFINED_BOOT_SECTOR };
int mounted_partition = NO_MOUNTED_PARTITION;

boolean file_system_initialized = false;

typedef struct t2fs_superbloco SuperBloco;
typedef struct t2fs_record FileRecord;
typedef struct t2fs_inode iNode;

typedef struct t_open_file 
{
    FileRecord record;
    DWORD current_pointer;
} OpenFile;

DWORD open_file_inodes[MAX_OPEN_FILES];
DWORD open_dir_inodes[MAX_OPEN_DIRS];
DWORD open_file_pointer_positions[MAX_OPEN_FILES];

OpenFile open_files[MAX_OPEN_FILES];
OpenFile open_directories[MAX_OPEN_DIRS];


void initialize_file_system();

boolean verify_file_handle( FILE2 handle );

boolean verify_dir_handle( DIR2 handle );

FILE2 retrieve_free_file_handle();

DIR2 retrieve_free_dir_handle();

int retrieve_inode( DWORD inode_number, iNode* inode );

int read_n_bytes_from_file( DWORD pointer, int n, iNode inode, char* buffer );

int write_n_bytes_to_file( DWORD pointer, int n, iNode inode, char* buffer );

int retrieve_dir_record( char* path, FileRecord* record );