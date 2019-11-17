#include "../include/support.h"

void initialize_file_system()
{
    if (file_system_initialized)
        return;

    mounted_partition_index = NO_MOUNTED_PARTITION;

    is_the_root_dir_open = false;

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

    // Inicializa os campos necessários dos arquivos abertos
    int i;
    for (i = 0; i < MAX_OPEN_FILES; i++)
    {
        open_files[i].handle_used = HANDLE_UNUSED;
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

    for (int i = 0; i < partitions[partition].number_of_inodes; i++)
    {
        setBitmap2(BITMAP_INODE, i, 0);
    }

    for (int i = 0; i < partitions[partition].number_of_data_blocks; i++)
    {
        setBitmap2(BITMAP_DADOS, i, 0);
    }

    if (closeBitmap2() != SUCCESS)
        return ERROR;

    return SUCCESS;
}

void define_empty_inode_from_inode_pointer(iNode *inode_pointer)
{
    inode_pointer->blocksFileSize = (DWORD)0;
    inode_pointer->bytesFileSize = (DWORD)0;
    inode_pointer->dataPtr[0] = POINTER_UNUSED;
    inode_pointer->dataPtr[1] = POINTER_UNUSED;
    inode_pointer->singleIndPtr = POINTER_UNUSED;
    inode_pointer->doubleIndPtr = POINTER_UNUSED;
    inode_pointer->RefCounter = (DWORD)0;
}

int format_root_dir(int partition)
{
    if (openBitmap2(partitions[partition].boot_sector) != SUCCESS)
        return ERROR;

    setBitmap2(BITMAP_INODE, 0, 1); // Inode da raiz está ocupado

    if (closeBitmap2() != SUCCESS)
        return ERROR;

    iNode inode;

    define_empty_inode_from_inode_pointer(&inode);

    return SUCCESS;
}

DWORD checksum(int partition) // Verificar se está funcionando
{
    DWORD checksum = ~(((DWORD *)&(partitions[partition].super_block))[0] +
                       ((DWORD *)&(partitions[partition].super_block))[1] +
                       ((DWORD *)&(partitions[partition].super_block))[2] +
                       ((DWORD *)&(partitions[partition].super_block))[3] +
                       ((DWORD *)&(partitions[partition].super_block))[4]);
    return checksum;
}

DWORD get_inode_number_from_file_using_filename(char *filename)
{
    return INVALID_INODE_NUMBER;
}

boolean is_a_handle_used(FILE2 handle)
{
    if (handle < 0 || handle >= MAX_OPEN_FILES)
        return false;

    if (open_files[handle].handle_used == HANDLE_UNUSED)
        return false;

    return true;
}

FILE2 retrieve_free_handle()
{
    FILE2 handle;
    for (handle = 0; handle < MAX_OPEN_FILES; handle++)
    {
        if (open_files[handle].handle_used == HANDLE_UNUSED)
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