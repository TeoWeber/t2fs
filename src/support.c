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

boolean is_a_handle_used(FILE2 handle)
{
    if (handle < 0 || handle >= MAX_OPEN_FILES)
        return false;

    if (open_files[handle].handle_used == HANDLE_UNUSED)
        return false;

    return true;
}

Record *get_record_pointer_from_file_given_filename(char *filename)
{
    // Salvamos contexto do root dir (aberto/fechado) (entrada apontada)
    boolean was_the_root_dir_open = is_the_root_dir_open;
    int root_dir_entry_late_pointer = root_dir_entry_current_pointer;

    // Abrimos root dir em sua primeira entrada
    is_the_root_dir_open = true;
    root_dir_entry_current_pointer = 0;

    // Percorremos todas entradas em busca de uma entrada com nome == filename
    boolean hitFlag = false;
    DIRENT2 dentry;
    while (readdir2(&dentry) == SUCCESS)
    {
        if (strcmp(dentry.name, filename) == 0) // Se as strings são iguais strcmp retorna 0
        {
            hitFlag = true;
            break;
        }
    }

    // Restauramos contexto do root dir (aberto/fechado) (entrada apontada)
    is_the_root_dir_open = was_the_root_dir_open;
    root_dir_entry_current_pointer = root_dir_entry_late_pointer;

    // Se não encontrou nenhuma entrada na busca anterior, retornamos falha
    if (!hitFlag)
        return INVALID_RECORD_POINTER;

    // Se o tipo da entrada não for válido, retornamos falha
    if (dentry.fileType != TYPEVAL_REGULAR && dentry.fileType != TYPEVAL_LINK)
        return INVALID_RECORD_POINTER;

    // FUNÇÃO NÃO FINALIZADA
}

FILE2 get_first_unused_handle()
{
    FILE2 handle;
    for (handle = 0; handle < MAX_OPEN_FILES; handle++)
    {
        if (open_files[handle].handle_used == HANDLE_UNUSED)
            return handle;
    }

    return INVALID_HANDLE;
}

iNode *get_inode_pointer_given_inode_number(DWORD inode_number)
{
    return INVALID_INODE_POINTER;
}

int read_n_bytes_from_file(DWORD pointer, int n, iNode inode, char *buffer)
{
}

int write_n_bytes_to_file(DWORD pointer, int n, iNode inode, char *buffer)
{
}

int strcmp(const char *s1, const char *s2)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (*p1 != '\0')
    {
        if (*p2 == '\0')
            return 1;
        if (*p2 > *p1)
            return -1;
        if (*p1 > *p2)
            return 1;

        p1++;
        p2++;
    }

    if (*p2 != '\0')
        return -1;

    return 0;
}