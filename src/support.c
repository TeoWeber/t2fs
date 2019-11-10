#include "support.h"

void initialize_file_system()
{
    if ( file_system_initialized )
        return;

    // inicializa arquivos abertos
    int i;
    for ( i = 0; i < MAX_OPEN_FILES; i++ )
    {
        open_file_inodes[i] = FILE_HANDLE_UNUSED;
        open_files[i].current_pointer = POINTER_START_POSITION;
        open_files[i].record.inodeNumber = INVALID_POINTER;
        open_files[i].record.TypeVal = TYPEVAL_INVALIDO;
    }

    // inicializa diretórios abertos
    for ( i = 0; i < MAX_OPEN_DIRS; i++ )
    {
        open_dir_inodes[i] = DIR_HANDLE_UNUSED;
        open_directories[i].current_pointer = POINTER_START_POSITION;
        open_directories[i].record.inodeNumber = INVALID_POINTER;
        open_directories[i].record.TypeVal = TYPEVAL_INVALIDO;
    }

    file_system_initialized = true;
}

boolean verify_file_handle( FILE2 handle )
{
    if ( handle < 0 || handle >= MAX_OPEN_FILES )
        return false;
    
    if ( open_file_inodes[handle] == FILE_HANDLE_UNUSED )
        return false;

    return true;
}

boolean verify_dir_handle( DIR2 handle )
{
    if ( handle < 0 || handle >= MAX_OPEN_DIRS )
        return false;

    if ( open_dir_inodes[handle] == DIR_HANDLE_UNUSED )
        return false;

    return true;
}

FILE2 retrieve_free_file_handle()
{
    FILE2 handle;
    for ( handle = 0; handle < MAX_OPEN_FILES; handle++ )
    {
        if ( open_file_inodes[handle] == FILE_HANDLE_UNUSED )
            return handle;
    }

    return INVALID_HANDLE;
}

DIR2 retrieve_free_dir_handle()
{
    DIR2 handle;
    for ( handle = 0; handle < MAX_OPEN_DIRS; handle++ )
    {
        if ( open_file_inodes[handle] == DIR_HANDLE_UNUSED )
            return handle;
    }

    return INVALID_HANDLE;
}

int retrieve_inode( DWORD inode_number, iNode* inode )
{

}

int read_n_bytes_from_file( DWORD pointer, int n, iNode inode, char* buffer )
{

}

int write_n_bytes_to_file( DWORD pointer, int n, iNode inode, char* buffer )
{

}

int retrieve_dir_record( char* path, FileRecord* record )
{
    iNode inode;
    char dir_name[MAX_FILE_NAME_SIZE + 1];

    return SUCCESS;
}