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

int fill_partition_structure(int partition, int sectors_per_block)
{
	if (sectors_per_block <= 0) // Tamanho de bloco inválido
		return ERROR;
    if (sectors_per_block > 65535) // Overflow em futuro (WORD) casting
        return ERROR;
	if (sectors_per_block > partitions[partition].size_in_sectors) // Resultaria em uma partição com menos de um bloco
		return ERROR;

    strcopy(partitions[partition].super_block.id, "T2FS");

    partitions[partition].super_block.version = (WORD)0x7E32;

    partitions[partition].super_block.superblockSize = (WORD)1;

    partitions[partition].super_block.blockSize = (WORD)sectors_per_block;

    partitions[partition].super_block.diskSize = (DWORD)(partitions[partition].size_in_sectors /
                                                         sectors_per_block);
    DWORD remainingBlocks = partitions[partition].super_block.diskSize - (DWORD)1;

    // Deveríamos testar se a seguinte operação não dá casting overflow.
    // Mas dado o tamanho do disco (1MB), isso não tem como ocorrer se o MBR estiver corretamente preenchido.
    partitions[partition].super_block.inodeAreaSize = (WORD)((remainingBlocks + (DWORD)(10 - 1)) / 
                                                             (DWORD)10);
    remainingBlocks -= (DWORD)partitions[partition].super_block.inodeAreaSize;

    partitions[partition].super_block.freeInodeBitmapSize = (partitions[partition].super_block.inodeAreaSize + (WORD)(8 * sizeof(iNode) - 1)) /
                                                            (WORD)(8 * sizeof(iNode));
    remainingBlocks -= (DWORD)partitions[partition].super_block.freeInodeBitmapSize;

    // Deveríamos testar se a seguinte operação não dá casting overflow.
    // Mas dado o tamanho do disco (1MB), isso não tem como ocorrer se o MBR estiver corretamente preenchido.
    partitions[partition].super_block.freeBlocksBitmapSize = (WORD)((remainingBlocks + (DWORD)(8 * SECTOR_SIZE + 1 - 1)) /
                                                                    (DWORD)(8 * SECTOR_SIZE + 1));
    remainingBlocks -= (DWORD)partitions[partition].super_block.freeBlocksBitmapSize;

    partitions[partition].super_block.Checksum = checksum(partition);

    partitions[partition].number_of_inodes = partitions[partition].super_block.inodeAreaSize * sectors_per_block * SECTOR_SIZE / sizeof(iNode);

    partitions[partition].number_of_data_blocks = remainingBlocks;

    return SUCCESS;
}

int reset_bitmaps(int partition)
{
    if (openBitmap2(partitions[partition].boot_sector) != SUCCESS)
        return ERROR;

    for (int i = 1; i <= partitions[partition].number_of_inodes; i++)
    {
        setBitmap2(BITMAP_INODE, i, 0);
    }

    for (int i = 1; i <= partitions[partition].number_of_data_blocks; i++)
    {
        setBitmap2(BITMAP_DADOS, i, 0);
    }

    if (closeBitmap2() != SUCCESS)
        return ERROR;

    return SUCCESS;
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

boolean is_a_file_handle_used(FILE2 handle)
{
    if (handle < 0 || handle >= MAX_OPEN_FILES)
        return false;

    if (open_files[handle].handle_used == FILE_HANDLE_UNUSED)
        return false;

    return true;
}

boolean is_a_dir_handle_used(DIR2 handle)
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