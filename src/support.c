#include "../include/support.h"

void initialize_file_system()
{
    if (file_system_initialized)
        return;

    mounted_partition_index = NO_MOUNTED_PARTITION;

    is_the_root_dir_open = false;

    // Inicializa o mbr
    read_sector(0, (unsigned char *)&mbr);

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

    strcpy(partitions[partition].super_block.id, "T2FS");

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
    {
        closeBitmap2();
        return ERROR;
    }

    for (int i = 0; i < partitions[partition].number_of_inodes; i++)
    {
        if (setBitmap2(BITMAP_INODE, i, 0) != SUCCESS)
        {
            closeBitmap2();
            return ERROR;
        }
    }

    for (int i = 0; i < partitions[partition].number_of_data_blocks; i++)
    {
        if (setBitmap2(BITMAP_DADOS, i, 0) != SUCCESS)
        {
            closeBitmap2();
            return ERROR;
        }
    }

    if (closeBitmap2() != SUCCESS)
        return ERROR;

    return SUCCESS;
}

void define_empty_inode_from_inode_ptr(iNode *inode_ptr)
{
    inode_ptr->blocksFileSize = (DWORD)0;
    inode_ptr->bytesFileSize = (DWORD)0;
    inode_ptr->dataPtr[0] = INVALID_PTR;
    inode_ptr->dataPtr[1] = INVALID_PTR;
    inode_ptr->singleIndPtr = INVALID_PTR;
    inode_ptr->doubleIndPtr = INVALID_PTR;
    inode_ptr->RefCounter = (DWORD)0;
}

int format_root_dir(int partition)
{
    if (openBitmap2(partitions[partition].boot_sector) != SUCCESS)
    {
        closeBitmap2();
        return ERROR;
    }

    // Setamos inode da raiz como ocupado
    if (setBitmap2(BITMAP_INODE, 0, 1) != SUCCESS)
    {
        closeBitmap2();
        return ERROR;
    }

    if (closeBitmap2() != SUCCESS)
        return ERROR;

    iNode inode;
    define_empty_inode_from_inode_ptr(&inode);

    DWORD block_of_inodes_ptr;
    if (get_block_of_inodes_ptr_where_inode_should_be_given_inode_number(0) == INVALID_INODE_PTR)
        return ERROR;

    if (write_inode_in_i_th_position_of_block_of_inodes(block_of_inodes_ptr, inode, 0) != SUCCESS)
        return ERROR;

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

Record *get_record_ptr_from_file_given_filename(char *filename)
{
    // Percorremos todas entradas em busca de uma entrada com nome == filename
    boolean hitFlag = false;
    Record *record_ptr;
    for (int i = 0; (record_ptr = get_i_th_record_ptr_from_root_dir(i)) != INVALID_RECORD_PTR; i++)
    {
        if (strcmp(record_ptr->name, filename) == 0) // Se as strings são iguais strcmp retorna 0
        {
            hitFlag = true;
            break;
        }
    }

    // Se não encontrou nenhuma entrada na busca anterior, retornamos falha
    if (!hitFlag)
        return INVALID_RECORD_PTR;

    // Se o tipo da entrada não for válido, retornamos falha
    if (record_ptr->TypeVal != TYPEVAL_REGULAR && record_ptr->TypeVal != TYPEVAL_LINK)
        return INVALID_RECORD_PTR;

    if (record_ptr->TypeVal == TYPEVAL_REGULAR)
        return record_ptr;

    // Então (record->TypeVal == TYPEVAL_LINK)
    DWORD unique_data_block_ptr = get_i_th_data_block_ptr_from_file_given_file_inode_number(0, record_ptr->inodeNumber);

    unsigned char unique_data_block[SECTOR_SIZE];
    if (read_sector(unique_data_block_ptr, unique_data_block) != SUCCESS)
        return INVALID_RECORD_PTR;

    return get_record_ptr_from_file_given_filename((char *)unique_data_block);
}

// Convenção de uso: O primeiro registro da root dir é o i-th registro, i == 0
Record *get_i_th_record_ptr_from_root_dir(DWORD i)
{
    DWORD number_of_records_per_data_blocks = (DWORD)partitions[mounted_partition_index].super_block.blockSize *
                                              SECTOR_SIZE /
                                              (DWORD)sizeof(Record);
    DWORD data_block_ptr = get_i_th_data_block_ptr_from_file_given_file_inode_number(i / number_of_records_per_data_blocks, 0);

    unsigned char data_block[SECTOR_SIZE];
    if (read_sector(data_block_ptr, data_block) != SUCCESS)
        return INVALID_RECORD_PTR;

    return &((Record *)data_block)[i % number_of_records_per_data_blocks];
}

// Convenção de uso: O primeiro bloco de dados de um arquivo é o i-th bloco de dados, i == 0
DWORD get_i_th_data_block_ptr_from_file_given_file_inode_number(DWORD i, DWORD inode_number)
{


    return INVALID_PTR;
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

iNode *get_inode_ptr_given_inode_number(DWORD inode_number)
{
    return INVALID_INODE_PTR;
}

iNode *allocate_next_free_inode_given_itself_and_get_ptr(iNode inode)
{
    return INVALID_INODE_PTR;
}

int alocate_next_free_data_block_to_file_given_file_inode(iNode inode)
{
    DWORD block_of_data_block_ptrs_capacity = partitions[mounted_partition_index].super_block.blockSize * (DWORD)SECTOR_SIZE / (DWORD)sizeof(DWORD);

    DWORD max_file_size_in_blocks = (DWORD)2 +
                                    block_of_data_block_ptrs_capacity +
                                    block_of_data_block_ptrs_capacity * block_of_data_block_ptrs_capacity;

    if (inode.blocksFileSize >= max_file_size_in_blocks)
        return ERROR;

    if (openBitmap2(partitions[mounted_partition_index].boot_sector) != SUCCESS)
    {
        closeBitmap2();
        return ERROR;
    }

    DWORD next_free_block_ptr;
    if ((next_free_block_ptr = (DWORD)searchBitmap2(BITMAP_DADOS, 0)) == INVALID_PTR)
    {
        closeBitmap2();
        return ERROR;
    }

    if (setBitmap2(BITMAP_DADOS, next_free_block_ptr, 1) != SUCCESS)
    {
        closeBitmap2();
        return ERROR;
    }

    if (inode.blocksFileSize < (DWORD)2)
    {
        inode.dataPtr[inode.blocksFileSize] = next_free_block_ptr;
    }
    else
    {
        if (inode.blocksFileSize < (DWORD)2 + block_of_data_block_ptrs_capacity)
        {
            if (inode.blocksFileSize == (DWORD)2)
            {
                inode.singleIndPtr = next_free_block_ptr;

                if ((next_free_block_ptr = (DWORD)searchBitmap2(BITMAP_DADOS, 0)) == INVALID_PTR)
                {
                    closeBitmap2();
                    return ERROR;
                }

                if (setBitmap2(BITMAP_DADOS, next_free_block_ptr, 1) != SUCCESS)
                {
                    closeBitmap2();
                    return ERROR;
                }
            }

            if (write_data_block_ptr_in_i_th_position_of_block_of_data_block_ptrs(inode.singleIndPtr, next_free_block_ptr, inode.blocksFileSize - (DWORD)2) != SUCCESS)
            {
                closeBitmap2();
                return ERROR;
            }
        }
        else
        {
            if (inode.blocksFileSize == (DWORD)2 + block_of_data_block_ptrs_capacity)
            {
                inode.doubleIndPtr = next_free_block_ptr;

                if ((next_free_block_ptr = (DWORD)searchBitmap2(BITMAP_DADOS, 0)) == INVALID_PTR)
                {
                    closeBitmap2();
                    return ERROR;
                }

                if (setBitmap2(BITMAP_DADOS, next_free_block_ptr, 1) != SUCCESS)
                {
                    closeBitmap2();
                    return ERROR;
                }
            }

            DWORD block_of_data_block_ptrs;
            if ((block_of_data_block_ptrs = read_data_block_ptr_from_i_th_position_of_block_of_data_block_ptrs(inode.doubleIndPtr,
                                                                                                               (inode.blocksFileSize - (DWORD)2 - block_of_data_block_ptrs_capacity) /
                                                                                                                   block_of_data_block_ptrs_capacity)) == INVALID_PTR)
            {
                closeBitmap2();
                return ERROR;
            }

            if (write_data_block_ptr_in_i_th_position_of_block_of_data_block_ptrs(block_of_data_block_ptrs,
                                                                                  next_free_block_ptr,
                                                                                  (inode.blocksFileSize - (DWORD)2 - block_of_data_block_ptrs_capacity) %
                                                                                      block_of_data_block_ptrs_capacity) != SUCCESS)
            {
                closeBitmap2();
                return ERROR;
            }
        }
    }

    inode.blocksFileSize++;

    if (closeBitmap2() != SUCCESS)
        return ERROR;

    return SUCCESS;
}

int read_n_bytes_from_file(DWORD ptr, int n, iNode inode, char *buffer)
{
    int read_bytes = 0;
    int sector;
    char sector_buffer[SECTOR_SIZE];

    while ( read_bytes < n )
    {
        int remaining_bytes = n - read_bytes;
        int success; 

        // encontrar setor
        success = read_sector( sector, sector_buffer );
        if ( success != 0 )
            return ERROR;

        if ( remaining_bytes >= 256 )
        {
            strcat( buffer, sector_buffer );
            read_bytes += 256;
        }
        else
        {
            int i;
            for (i = 0; i < remaining_bytes; i++)
                buffer[read_bytes+i-1] = sector_buffer[i];
            read_bytes += remaining_bytes;
        }
    }

    return read_bytes;
}

int write_n_bytes_to_file(DWORD ptr, int n, iNode inode, char *buffer)
{
    return ERROR;
}

DWORD get_block_of_inodes_ptr_where_inode_should_be_given_inode_number(DWORD inode_number)
{
    return INVALID_PTR;
}

DWORD get_data_block_ptr_where_data_should_be_given_data_number_of_block(DWORD data_number_of_block)
{
    return INVALID_PTR;
}

// Convenção de uso: O primeiro inode do bloco de inodes é o i-th inode, i == 0
iNode *get_inode_from_in_i_th_position_of_block_of_inodes(DWORD block_of_inodes_ptr, DWORD i)
{
    return INVALID_INODE_PTR;
}

// Convenção de uso: O primeiro inode do bloco de inodes é o i-th inode, i == 0
int write_inode_in_i_th_position_of_block_of_inodes(DWORD block_of_inodes_ptr, iNode inode, DWORD i)
{
    return ERROR;
}

// Convenção de uso: O primeiro ptr de bloco de dados da bloco de ponteiros de blocos de dados é o i-th ptr de blocos de dados, i == 0
DWORD get_data_block_ptr_from_i_th_position_of_block_of_data_block_ptrs(DWORD block_data_block_ptrs_ptr, DWORD i)
{
    return INVALID_PTR;
}

// Convenção de uso: O primeiro ptr de bloco de dados da bloco de ponteiros de blocos de dados é o i-th ptr de blocos de dados, i == 0
int write_data_block_ptr_in_i_th_position_of_block_of_data_block_ptrs(DWORD block_data_block_ptrs_ptr, DWORD data_block_ptr, DWORD i)
{
    return ERROR;
}