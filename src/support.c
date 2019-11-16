#include "../include/support.h"

void initialize_file_system()
{
    if (file_system_initialized)
        return;

    // Inicializa o mbr
    read_sector(0, (BYTE *)&mbr);

    // Inicializa o setor de boot das partições
    partitions[0].boot_sector = mbr.partition0BootSector;
    partitions[1].boot_sector = mbr.partition1BootSector;
    partitions[2].boot_sector = mbr.partition2BootSector;
    partitions[3].boot_sector = mbr.partition3BootSector;

    // Inicializa o setor final das partições
    partitions[0].final_sector = mbr.partition0FinalSector;
    partitions[1].final_sector = mbr.partition1FinalSector;
    partitions[2].final_sector = mbr.partition2FinalSector;
    partitions[3].final_sector = mbr.partition3FinalSector;

    // Inicializa o tamanho em número de setores das partições
    partitions[0].size_in_sectors = partitions[0].final_sector - partitions[0].boot_sector + (DWORD)1;
    partitions[1].size_in_sectors = partitions[1].final_sector - partitions[1].boot_sector + (DWORD)1;
    partitions[2].size_in_sectors = partitions[2].final_sector - partitions[2].boot_sector + (DWORD)1;
    partitions[3].size_in_sectors = partitions[3].final_sector - partitions[3].boot_sector + (DWORD)1;

    // Inicializa arquivos abertos
    int i;
    for (i = 0; i < MAX_OPEN_FILES; i++)
    {
        open_files[i].handle_used = FILE_HANDLE_UNUSED;
        open_files[i].current_pointer = POINTER_START_POSITION;
        open_files[i].record.inodeNumber = INVALID_POINTER;
        open_files[i].record.TypeVal = TYPEVAL_INVALIDO;
    }

    // Inicializa diretórios abertos
    for (i = 0; i < MAX_OPEN_DIRS; i++)
    {
        open_dir_inodes[i] = DIR_HANDLE_UNUSED;
        open_directories[i].current_pointer = POINTER_START_POSITION;
        open_directories[i].record.inodeNumber = INVALID_POINTER;
        open_directories[i].record.TypeVal = TYPEVAL_INVALIDO;
    }

    file_system_initialized = true;
}

DWORD checksum(int partition) // Verificar se está funcionando
{
    unsigned long long int gross_checksum = ((DWORD *)&(partitions[partition].super_block))[0] +
                                            ((DWORD *)&(partitions[partition].super_block))[1] +
                                            ((DWORD *)&(partitions[partition].super_block))[2] +
                                            ((DWORD *)&(partitions[partition].super_block))[3] +
                                            ((DWORD *)&(partitions[partition].super_block))[4];

    DWORD checksum = ~((DWORD)gross_checksum + (DWORD)(gross_checksum >> 32));

    return checksum;
}

boolean verify_file_handle(FILE2 handle)
{
    if (handle < 0 || handle >= MAX_OPEN_FILES)
        return false;

    if (open_files[handle].handle_used == FILE_HANDLE_UNUSED)
        return false;

    return true;
}

boolean verify_dir_handle(DIR2 handle)
{
    if (handle < 0 || handle >= MAX_OPEN_DIRS)
        return false;

    if (open_dir_inodes[handle] == DIR_HANDLE_UNUSED)
        return false;

    return true;
}

FILE2 retrieve_free_file_handle()
{
    FILE2 handle;
    for (handle = 0; handle < MAX_OPEN_FILES; handle++)
    {
        if (open_files[handle].handle_used == FILE_HANDLE_UNUSED)
            return handle;
    }

    return INVALID_HANDLE;
}

DIR2 retrieve_free_dir_handle()
{
    DIR2 handle;
    for (handle = 0; handle < MAX_OPEN_DIRS; handle++)
    {
        if (open_files[handle].handle_used == DIR_HANDLE_UNUSED)
            return handle;
    }

    return INVALID_HANDLE;
}

int retrieve_inode(DWORD inode_number, iNode *inode)
{
}

int read_n_bytes_from_file(DWORD pointer, int n, iNode inode, char *buffer)
{
}

int write_n_bytes_to_file(DWORD pointer, int n, iNode inode, char *buffer)
{
}

int retrieve_dir_record(char *path, FileRecord *record)
{
    iNode inode;
    char dir_name[MAX_FILE_NAME_SIZE + 1];

    return SUCCESS;
}