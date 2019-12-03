#include "support.h"

boolean file_system_initialized = false;

MBR mbr;

Partition partitions[MAX_PARTITIONS];
int mounted_partition_index;

boolean is_the_root_dir_open;
DWORD root_dir_entry_current_ptr;
iNode *root_dir_inode_ptr;

OpenFile open_files[MAX_OPEN_FILES];

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

int reset_partition_sectors(int partition)
{
    unsigned char emptyBuffer[SECTOR_SIZE];
    memset((void *)emptyBuffer, '\0', SECTOR_SIZE);
    for (DWORD sector_number = partitions[partition].boot_sector; sector_number <= partitions[partition].final_sector; sector_number++)
    {
        if ((write_sector(sector_number, emptyBuffer)) != SUCCESS)
            return ERROR;
    }
    return SUCCESS;
}

int fill_partition_structure(int partition, int sectors_per_block)
{
    if (sectors_per_block <= 0) // Tamanho de bloco inválido
        return ERROR;
    if (sectors_per_block > 65535) // Overflow em futuro (WORD) casting
        return ERROR;
    if ((DWORD)sectors_per_block > partitions[partition].size_in_sectors) // Resultaria em uma partição com menos de um bloco
        return ERROR;

    BYTE superblock_id[] = "T2FS";
    memcpy(partitions[partition].super_block.id, superblock_id, 4);

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

    for (int i = 0; (DWORD)i < partitions[partition].number_of_inodes; i++)
    {
        if (setBitmap2(BITMAP_INODE, i, 0) != SUCCESS)
        {
            closeBitmap2();
            return ERROR;
        }
    }

    for (int i = 0; (DWORD)i < partitions[partition].number_of_data_blocks; i++)
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

    update_inode_on_disk(0, inode);

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
    Record *record_ptr;
    for (int i = 0; 1; i++)
    {
        record_ptr = get_i_th_record_ptr_from_root_dir(i);
        if (record_ptr == INVALID_RECORD_PTR)
            return INVALID_RECORD_PTR;

        if (!is_used_record_ptr(record_ptr))
        {
            free(record_ptr);
            return INVALID_RECORD_PTR;
        }

        if (strcmp(record_ptr->name, filename) == 0) // Se as strings são iguais strcmp retorna 0
            break;

        free(record_ptr);
    }

    // Se o tipo da entrada não for válido, retornamos falha
    if (record_ptr->TypeVal != TYPEVAL_REGULAR && record_ptr->TypeVal != TYPEVAL_LINK)
    {
        free(record_ptr);
        return INVALID_RECORD_PTR;
    }

    if (record_ptr->TypeVal == TYPEVAL_REGULAR)
        return record_ptr;

    // Então (record->TypeVal == TYPEVAL_LINK)
    DWORD unique_data_block_ptr = get_i_th_data_block_ptr_from_file_given_file_inode_number(0, record_ptr->inodeNumber);
    free(record_ptr);

    unsigned char unique_data_block[SECTOR_SIZE];
    if (read_sector(unique_data_block_ptr, unique_data_block) != SUCCESS)
        return INVALID_RECORD_PTR;

    return get_record_ptr_from_file_given_filename((char *)unique_data_block);
}

DWORD get_i_from_first_invalid_record()
{
    // Percorremos todas entradas em busca de uma entrada com nome == filename
    Record *record_ptr;
    int i;

    for (i = 0; 1; i++)
    {
        record_ptr = get_i_th_record_ptr_from_root_dir(i);
        if (record_ptr == INVALID_RECORD_PTR)
            return INVALID_RECORD_PTR;

        if (!is_used_record_ptr(record_ptr))
            break;

        free(record_ptr);
    }

    free(record_ptr);
    return (DWORD)i;
}

DWORD get_free_inode_number_in_partition()
{
    DWORD inode_number;

    if (openBitmap2(partitions[mounted_partition_index].boot_sector) != SUCCESS)
    { // Sem espaço livre!
        closeBitmap2();
        return 0; // ERROR
    }

    inode_number = searchBitmap2(BITMAP_INODE, 0); // Procura por uma posição vazia no Bitmap e retorna sua posição
    if (inode_number <= 0)
    { // Sem espaço livre!
        closeBitmap2();
        return 0; // ERROR
    }

    if (setBitmap2(BITMAP_INODE, inode_number, 1) != SUCCESS) // Ativa este inode no Bitmap da partição
    {
        closeBitmap2();
        return 0; // ERROR
    }

    if (closeBitmap2() != SUCCESS)
        return 0; // ERROR

    return inode_number;
}

DWORD get_free_data_block_number_in_partition()
{
    DWORD data_block_number;

    if (openBitmap2(partitions[mounted_partition_index].boot_sector) != SUCCESS)
    { // Sem espaço livre!
        closeBitmap2();
        return 0; // ERROR
    }

    data_block_number = searchBitmap2(BITMAP_DADOS, 0); // Procura por uma posição vazia no Bitmap e retorna sua posição
    if (data_block_number <= 0)
    { // Sem espaço livre!
        closeBitmap2();
        return 0; // ERROR
    }

    if (setBitmap2(BITMAP_DADOS, data_block_number, 1) != SUCCESS) // Ativa este inode no Bitmap da partição
    {
        closeBitmap2();
        return 0; // ERROR
    }

    if (closeBitmap2() != SUCCESS)
        return 0; // ERROR

    return data_block_number;
}

// Convenção de uso: O primeiro registro da root dir é o i-th registro, i == 0
Record *get_i_th_record_ptr_from_root_dir(DWORD i)
{
    DWORD number_of_records_per_data_blocks = (DWORD)partitions[mounted_partition_index].super_block.blockSize *
                                              SECTOR_SIZE /
                                              (DWORD)sizeof(Record);
    DWORD data_block_ptr = get_i_th_data_block_ptr_from_file_given_file_inode_number(i / number_of_records_per_data_blocks, 0);

    int block_size_in_bytes = partitions[mounted_partition_index].super_block.blockSize * SECTOR_SIZE;
    char data_block[block_size_in_bytes];
    if (read_block_from_data_block_given_its_ptr(0, data_block_ptr, block_size_in_bytes, data_block) != block_size_in_bytes)
        return INVALID_RECORD_PTR;

    Record *record_ptr = (Record *)malloc(sizeof(Record));
    *record_ptr = ((Record *)data_block)[i % number_of_records_per_data_blocks];
    return record_ptr;
}

// Convenção de uso: O primeiro registro da root dir é o i-th registro, i == 0
int set_i_th_record_ptr_on_root_dir_given_itself(DWORD i, Record *record_ptr)
{
    DWORD number_of_records_per_data_blocks = (DWORD)partitions[mounted_partition_index].super_block.blockSize *
                                              SECTOR_SIZE /
                                              (DWORD)sizeof(Record);
    DWORD data_block_ptr = get_i_th_data_block_ptr_from_file_given_file_inode_number(i / number_of_records_per_data_blocks, 0);

    int block_size_in_bytes = partitions[mounted_partition_index].super_block.blockSize * SECTOR_SIZE;
    char data_block[block_size_in_bytes];
    if (read_block_from_data_block_given_its_ptr(0, data_block_ptr, block_size_in_bytes, data_block) != block_size_in_bytes)
        return ERROR;

    ((Record *)data_block)[i % number_of_records_per_data_blocks] = *record_ptr;

    if (write_block_of_data_to_data_block_given_its_ptr(data_block_ptr, data_block) != SUCCESS)
        return ERROR;

    return SUCCESS;
}

// Convenção de uso: O primeiro bloco de dados de um arquivo é o i-th bloco de dados, i == 0
DWORD get_i_th_data_block_ptr_from_file_given_file_inode_number(DWORD i, DWORD inode_number)
{
    iNode *inode;
    inode = get_inode_ptr_given_inode_number(inode_number);
    if (inode == (iNode *)INVALID_INODE_PTR)
        return INVALID_PTR;

    int block_size_in_bytes = partitions[mounted_partition_index].super_block.blockSize * SECTOR_SIZE;
    int ptr_per_block = block_size_in_bytes / sizeof(DWORD);

    if (i == 1)
    {
        DWORD ptr = inode->dataPtr[0];
        free(inode);
        return ptr;
    }
    else if (i == 2)
    {
        DWORD ptr = inode->dataPtr[1];
        free(inode);
        return ptr;
    }
    else if (i <= (DWORD)(ptr_per_block + 2))
    {
        DWORD ptrs[ptr_per_block];
        if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(inode.singleIndPtr, ptrs) != SUCCESS)
            return ERROR;

        free(inode);
        return ptrs[i - 3];
    }
    else if (i > (DWORD)(ptr_per_block + 2))
    {
        DWORD ind_ptrs[ptr_per_block];
        if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(inode.doubleIndPtr, ind_ptrs) != SUCCESS)
            return ERROR;

        int ptr_block_pos;
        i -= (ptr_per_block + 2);
        ptr_block_pos = i / ptr_per_block;

        DWORD ptrs[ptr_per_block];
        if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(ind_ptrs[ptr_block_pos], ptrs) != SUCCESS)
            return ERROR;

        int block_pos = i % ptr_per_block;
        free(inode);
        return ptrs[block_pos];
    }
    free(inode);
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
    SuperBlock super_block = partitions[mounted_partition_index].super_block;
    int block_size = super_block.blockSize;
    int super_block_size = super_block.superblockSize;
    int block_bitmap_size = super_block.freeBlocksBitmapSize;
    int inode_bitmap_size = super_block.freeInodeBitmapSize;

    int inode_start;
    int inode_disk_area = block_size * (super_block_size + block_bitmap_size + inode_bitmap_size);
    int sector = inode_disk_area + inode_number / 8;

    if (openBitmap2(partitions[mounted_partition_index].boot_sector) != SUCCESS)
    {
        closeBitmap2();
        return (iNode *)INVALID_INODE_PTR;
    }

    if (getBitmap2(BITMAP_INODE, inode_number) == 0)
    {
        closeBitmap2();
        return (iNode *)INVALID_INODE_PTR;
    }

    unsigned char sector_buffer[SECTOR_SIZE];
    if (read_sector(sector, sector_buffer) != 0)
    {
        closeBitmap2();
        return (iNode *)INVALID_INODE_PTR;
    }

    iNode *inode_ptr = (iNode *)malloc(sizeof(Record));
    inode_start = (inode_number % 8) * 32;
    inode_ptr->blocksFileSize = *((DWORD *)(sector_buffer + inode_start + 0));
    inode_ptr->bytesFileSize = *((DWORD *)(sector_buffer + inode_start + 4));
    inode_ptr->dataPtr[0] = *((DWORD *)(sector_buffer + inode_start + 8));
    inode_ptr->dataPtr[1] = *((DWORD *)(sector_buffer + inode_start + 12));
    inode_ptr->singleIndPtr = *((DWORD *)(sector_buffer + inode_start + 16));
    inode_ptr->doubleIndPtr = *((DWORD *)(sector_buffer + inode_start + 20));
    inode_ptr->RefCounter = *((DWORD *)(sector_buffer + inode_start + 24));
    inode_ptr->reservado = *((DWORD *)(sector_buffer + inode_start + 28));

    if (closeBitmap2() != SUCCESS)
        return (iNode *)INVALID_INODE_PTR;

    return inode_ptr;
}

int write_data_block_ptr_to_block_of_data_block_ptrs_given_its_ptr(int i, DWORD block_of_data_block_ptrs_ptr, DWORD ptr)
{
    unsigned char sector_buffer[SECTOR_SIZE];

    if (read_sector(block_of_data_block_ptrs_ptr + i * sizeof(DWORD) / SECTOR_SIZE, sector_buffer) != SUCCESS)
        return ERROR;

    insert_DWORD_value_in_its_position_on_buffer(ptr, (i % (SECTOR_SIZE / sizeof(DWORD))) * 4, sector_buffer);

    if (write_sector(block_of_data_block_ptrs_ptr + i * sizeof(DWORD) / SECTOR_SIZE, sector_buffer) != SUCCESS)
        return ERROR;

    return SUCCESS;
}

int get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(DWORD block_of_data_block_ptrs_ptr, DWORD *array_of_data_block_ptrs)
{
    unsigned char sector_buffer[SECTOR_SIZE];
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int ptr_per_sector = block_size * 4;

    int i;
    for (i = 0; i < block_size; i++)
    {
        if (read_sector(block_of_data_block_ptrs_ptr + i, sector_buffer) != SUCCESS)
            return ERROR;

        int j;
        for (j = 0; j < ptr_per_sector; j++)
            array_of_data_block_ptrs[j + i * ptr_per_sector] = *((DWORD *)(sector_buffer + j * sizeof(DWORD)));
    }
    return SUCCESS
}

int write_block_of_data_to_data_block_given_its_ptr(DWORD data_block_ptr, char *buffer)
{
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int written_bytes = 0;

    unsigned char sector_buffer[SECTOR_SIZE];
    for (int sector = 0; sector < block_size; sector++)
    {
        memset((void *)sector_buffer, '\0', SECTOR_SIZE);
        for (int i = 0; i < SECTOR_SIZE; i++)
        {
            sector_buffer[i] = buffer[written_bytes];
            written_bytes++;
        }

        if (write_sector(data_block_ptr + sector, sector_buffer) != SUCCESS)
            return ERROR;
    }
    return SUCCESS;
}

int read_block_from_data_block_given_its_ptr(int ptr, int data_block_ptr, int bytes, char *buffer)
{
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int read_bytes = 0;

    unsigned char sector_buffer[SECTOR_SIZE];
    for (int sector = ptr / SECTOR_SIZE; sector <  1 + (bytes + ptr - 1) / SECTOR_SIZE; sector++)
    {
        if (read_sector(data_block_ptr + sector, sector_buffer) == SUCCESS)
        {
            for (int i = 0; i < SECTOR_SIZE; i++)
            {
                if (read_bytes < bytes && sector * SECTOR_SIZE + i >= ptr) // ainda nao leu todos
                {
                    buffer[read_bytes] = sector_buffer[i];
                    read_bytes++;
                }
            }
        }
        else
            break;
    }

    return read_bytes;
}

int read_n_bytes_from_file_given_its_inode(DWORD ptr, int n, iNode inode, char *buffer)
{
    int read_bytes = 0, remaining_bytes = n;
    int curr_byte = 0;
    int block_size_in_bytes = partitions[mounted_partition_index].super_block.blockSize * SECTOR_SIZE;
    char aux_buff[block_size_in_bytes];

    int curr_block, curr_ptr;
    int ptr_per_block = block_size_in_bytes / sizeof(DWORD);
    curr_block = 1 + ptr / (block_size_in_bytes);
    curr_ptr = ptr - (curr_block - 1) * (block_size_in_bytes);

    // ponteiros diretos
    if (curr_block == 1)
    {
        read_bytes = read_block_from_data_block_given_its_ptr(curr_ptr, inode.dataPtr[curr_block - 1], n, aux_buff);
        int i;
        for (i = 0; i < read_bytes; i++)
        {
            if (remaining_bytes > 0)
            {
                buffer[curr_byte] = aux_buff[i];
                curr_byte += 1;
                remaining_bytes -= 1;
            }
        }

        ptr += read_bytes;
        if (remaining_bytes > 0)
            curr_block += 1;
    }

    if (curr_block == 2)
    {
        curr_ptr = ptr - (curr_block - 1) * (block_size_in_bytes);
        read_bytes = read_block_from_data_block_given_its_ptr(curr_ptr, inode.dataPtr[curr_block - 1], n, aux_buff);
        int i;
        for (i = 0; i < read_bytes; i++)
        {
            if (remaining_bytes > 0)
            {
                buffer[curr_byte] = aux_buff[i];
                curr_byte += 1;
                remaining_bytes -= 1;
            }
        }

        ptr += read_bytes;
        if (remaining_bytes > 0)
            curr_block += 1;
    }

    // ponteiro de indireção simples
    if (curr_block > 2 && curr_block <= ptr_per_block + 2)
    {
        DWORD ptrs[ptr_per_block];
        if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(inode.singleIndPtr, ptrs) != SUCCESS)
            return ERROR;

        int i;
        for (i = curr_block - 3; i < ptr_per_block; i++)
        {
            curr_ptr = ptr - (curr_block - 1) * (block_size_in_bytes);
            read_bytes = read_block_from_data_block_given_its_ptr(curr_ptr, inode.dataPtr[curr_block - 1], n, aux_buff);
            int j;
            for (j = 0; j < read_bytes; j++)
            {
                if (remaining_bytes > 0)
                {
                    buffer[curr_byte] = aux_buff[j];
                    curr_byte += 1;
                    remaining_bytes -= 1;
                }
            }

            ptr += read_bytes;
            curr_block = 1 + ptr / (block_size_in_bytes);
        }
    }

    if (remaining_bytes <= 0)
        return read_bytes;

    // ponteiro de indireção dupla
    if (curr_block > ptr_per_block + 2)
    {
        DWORD ptrs[ptr_per_block], ind_ptrs[ptr_per_block];
        if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(inode.doubleIndPtr, ind_ptrs) != SUCCESS)
            return ERROR;

        int first = (curr_block - ptr_per_block - 3) / ptr_per_block;
        int i;
        for (i = first; i < ptr_per_block && read_bytes < n; i++)
        {
            if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(ind_ptrs[i], ptrs) != SUCCESS)
                return ERROR;

            first = (curr_block - ptr_per_block - i - 3) - i * ptr_per_block;
            int j;
            for (j = first; j < ptr_per_block && read_bytes < n; j++)
            {
                curr_ptr = ptr - (curr_block - 1) * (block_size_in_bytes);
                read_bytes = read_block_from_data_block_given_its_ptr(curr_ptr, ptrs[j], n, aux_buff);

                int k;
                for (k = 0; k < read_bytes; k++)
                {
                    if (remaining_bytes > 0)
                    {
                        buffer[curr_byte] = aux_buff[k];
                        curr_byte += 1;
                        remaining_bytes -= 1;
                    }

                    ptr += read_bytes;
                    if (remaining_bytes > 0)
                        curr_block = 1 + ptr / (block_size_in_bytes);
                }
            }
        }
    }

    // falta: caso em que é um softlink

    return read_bytes;
}

int initialize_new_block_of_data_block_ptrs_and_get_its_number()
{
    int block_size = partitions[mounted_partition_index].super_block.blockSize;

    DWORD data_block_number_aux_buffer;
    DWORD data_block_ptr_aux_buffer;
    if ((data_block_number_aux_buffer = get_free_data_block_number_in_partition()) == 0)
        return ERROR;
    if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
        return ERROR;

    char emptyBuffer[SECTOR_SIZE];
    memset((void *)emptyBuffer, '\0', SECTOR_SIZE);

    for (int i = 0; i < block_size; i++)
    {
        int success = write_sector(data_block_ptr_aux_buffer, (unsigned char *)emptyBuffer);
        if (success != 0)
            return ERROR;
    }

    return data_block_number_aux_buffer;
}

void insert_DWORD_value_in_its_position_on_buffer(DWORD dword_value, int starting_pos, unsigned char *block_buffer)
{
    unsigned char *aux_ptr = (unsigned char *)&dword_value;

    block_buffer[starting_pos] = aux_ptr[0];
    block_buffer[starting_pos + 1] = aux_ptr[1];
    block_buffer[starting_pos + 2] = aux_ptr[2];
    block_buffer[starting_pos + 3] = aux_ptr[3];
}

int write_n_bytes_to_file_given_its_inode_number(DWORD ptr, int n, int inode_number, char *buffer)
{
    iNode *inode;
    if ((inode = get_inode_ptr_given_inode_number(inode_number)) == (iNode *)INVALID_INODE_PTR)
        return ERROR;

    int written_bytes = 0, remaining_bytes = n;
    int block_size_in_bytes = partitions[mounted_partition_index].super_block.blockSize * SECTOR_SIZE;
    int ptr_per_block = block_size_in_bytes / sizeof(DWORD);

    char aux_buff[block_size_in_bytes];

    int curr_block, curr_ptr;
    DWORD first_ptr = ptr;

    curr_block = 1 + ptr / (block_size_in_bytes);
    curr_ptr = ptr - (curr_block - 1) * block_size_in_bytes;

    DWORD data_block_number_aux_buffer;
    DWORD data_block_ptr_aux_buffer;
    // ponteiros diretos
    if (curr_block == 1)
    {
        if (inode->dataPtr[0] == INVALID_PTR)
        {
            if ((data_block_number_aux_buffer = get_free_data_block_number_in_partition()) == 0)
                return ERROR;
            if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
                return ERROR;
            inode->dataPtr[0] = data_block_ptr_aux_buffer;
            inode->blocksFileSize += 1;
        }

        int i;
        for (i = curr_ptr; i < block_size_in_bytes && remaining_bytes > 0; i++)
        {
            aux_buff[i] = buffer[written_bytes];
            written_bytes += 1;
            remaining_bytes -= 1;
        }

        if (write_block_of_data_to_data_block_given_its_ptr(inode->dataPtr[0], aux_buff) != SUCCESS)
        {
            free(inode);
            return ERROR;
        }

        if (remaining_bytes > 0)
            curr_block += 1;
        ptr += written_bytes;
    }

    if (curr_block == 2)
    {
        curr_ptr = ptr - (curr_block - 1) * block_size_in_bytes;
        if (inode->dataPtr[1] == INVALID_PTR)
        {
            if ((data_block_number_aux_buffer = get_free_data_block_number_in_partition()) == 0)
                return ERROR;
            if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
                return ERROR;
            inode->dataPtr[1] = data_block_ptr_aux_buffer;
            inode->blocksFileSize += 1;
        }

        int i;
        for (i = curr_ptr; i < block_size_in_bytes && remaining_bytes > 0; i++)
        {
            aux_buff[i] = buffer[written_bytes];
            written_bytes += 1;
            remaining_bytes -= 1;
        }

        if (write_block_of_data_to_data_block_given_its_ptr(inode->dataPtr[1], aux_buff) != SUCCESS)
        {
            free(inode);
            return ERROR;
        }

        if (remaining_bytes > 0)
            curr_block += 1;
        ptr += written_bytes;
    }

    // ponteiros de indireção simples
    if (curr_block > 2 && curr_block <= ptr_per_block + 2)
    {
        // inicializa novo bloco de ponteiros simples se necessário
        if (inode->singleIndPtr == INVALID_PTR)
        {
            DWORD data_block_number_aux_buffer;
            DWORD data_block_ptr_aux_buffer;
            if ((data_block_number_aux_buffer = initialize_new_block_of_data_block_ptrs_and_get_its_number()) == 0)
                return ERROR;
            if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
                return ERROR;
            inode->singleIndPtr = data_block_ptr_aux_buffer;
        }

        DWORD ptrs[ptr_per_block];
        if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(inode.singleIndPtr, ptrs) != SUCCESS)
            return ERROR;

        int i;
        for (i = curr_block - 3; i < ptr_per_block; i++)
        {
            if (remaining_bytes > 0)
            {
                curr_ptr = ptr - (curr_block - 1) * block_size_in_bytes;
                if (ptrs[i] == INVALID_PTR)
                {
                    if ((data_block_number_aux_buffer = get_free_data_block_number_in_partition()) == 0)
                        return ERROR;
                    if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
                        return ERROR;
                    ptrs[i] = data_block_ptr_aux_buffer;
                    inode->blocksFileSize += 1;

                    write_data_block_ptr_to_block_of_data_block_ptrs_given_its_ptr(i, inode->singleIndPtr, ptrs[i]);
                }

                int j;
                for (j = curr_ptr; j < block_size_in_bytes && remaining_bytes > 0; j++)
                {
                    aux_buff[j] = buffer[written_bytes];
                    written_bytes += 1;
                    remaining_bytes -= 1;
                }

                if (write_block_of_data_to_data_block_given_its_ptr(ptrs[i], aux_buff) != SUCCESS)
                {
                    free(inode);
                    return ERROR;
                }

                if (remaining_bytes > 0)
                    curr_block += 1;
                ptr = first_ptr + written_bytes;
            }
        }
    }

    // ponteiros de indireção dupla
    if (curr_block > ptr_per_block + 2)
    {
        // inicializa novo bloco de ponteiros duplos se necessário
        if (inode->doubleIndPtr == INVALID_PTR)
        {
            DWORD data_block_number_aux_buffer;
            DWORD data_block_ptr_aux_buffer;
            if ((data_block_number_aux_buffer = initialize_new_block_of_data_block_ptrs_and_get_its_number()) == 0)
                return ERROR;
            if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
                return ERROR;
            inode->doubleIndPtr = data_block_ptr_aux_buffer;
        }

        DWORD ind_ptrs[ptr_per_block];
        if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(inode.doubleIndPtrs, ind_ptrs) != SUCCESS)
            return ERROR;

        int first = (curr_block - ptr_per_block - 3) / ptr_per_block;
        int i;
        for (i = first; i < ptr_per_block; i++)
        {
            if (remaining_bytes > 0)
            {
                if (ind_ptrs[i] == INVALID_PTR)
                {
                    DWORD data_block_number_aux_buffer;
                    DWORD data_block_ptr_aux_buffer;
                    if ((data_block_number_aux_buffer = initialize_new_block_of_data_block_ptrs_and_get_its_number()) == 0)
                        return ERROR;
                    if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
                        return ERROR;
                    ind_ptrs[i] = data_block_ptr_aux_buffer;

                    write_data_block_ptr_to_block_of_data_block_ptrs_given_its_ptr(i, inode->doubleIndPtr, ind_ptrs[i]);
                }

                DWORD ptrs[ptr_per_block];
                if (get_data_block_ptrs_from_block_of_data_block_ptrs_given_its_ptr(ind_ptrs[i], ptrs) != SUCCESS)
                    return ERROR;

                first = (curr_block - ptr_per_block - 3) - i * ptr_per_block;
                int j;
                for (j = first; j < ptr_per_block; j++)
                {
                    if (remaining_bytes > 0)
                    {
                        curr_ptr = ptr - (curr_block - 1) * block_size_in_bytes;
                        if (ptrs[j] == INVALID_PTR)
                        {
                            if ((data_block_number_aux_buffer = get_free_data_block_number_in_partition()) == 0)
                                return ERROR;
                            if ((data_block_ptr_aux_buffer = get_data_block_ptr_given_data_block_number(data_block_number_aux_buffer)) == INVALID_PTR)
                                return ERROR;
                            ptrs[j] = data_block_ptr_aux_buffer;
                            inode->blocksFileSize += 1;

                            write_data_block_ptr_to_block_of_data_block_ptrs_given_its_ptr(j, ind_ptrs[i], ptrs[j]);
                        }

                        int k;
                        for (k = curr_ptr; k < block_size_in_bytes && remaining_bytes > 0; k++)
                        {
                            aux_buff[k] = buffer[written_bytes];
                            written_bytes += 1;
                            remaining_bytes -= 1;
                        }

                        if (write_block_of_data_to_data_block_given_its_ptr(ptrs[i], aux_buff) != SUCCESS)
                        {
                            free(inode);
                            return ERROR;
                        }

                        if (remaining_bytes > 0)
                            curr_block += 1;

                        curr_ptr = first_ptr + written_bytes;
                    }
                }
            }
        }
    }

    if ((DWORD)curr_ptr > inode->bytesFileSize)
        inode->bytesFileSize = curr_ptr;
    update_inode_on_disk(inode_number, *inode);

    free(inode);

    return written_bytes;
}

boolean is_used_record_ptr(Record *record_ptr)
{
    char emptyRecord[sizeof(Record)];
    memset((void *)emptyRecord, '\0', sizeof(Record));

    if (strcmp((char *)record_ptr, (char *)emptyRecord) == 0)
        return false;
    else
        return true;
}

int ghost_create2(char *filename)
{
    initialize_file_system();

    if (mounted_partition_index == NO_MOUNTED_PARTITION)
        return ERROR;

    if (get_record_ptr_from_file_given_filename(filename) == INVALID_RECORD_PTR) // Não tinha ninguém com esse filename
    {
        DWORD new_inode_id = get_free_inode_number_in_partition();
        if (new_inode_id == 0)
            return ERROR;

        iNode new_inode;
        define_empty_inode_from_inode_ptr(&new_inode);
        new_inode.RefCounter = 1;

        update_inode_on_disk(new_inode_id, new_inode);

        DWORD new_record_id = get_i_from_first_invalid_record();

        Record *new_record_ptr = (Record *)malloc(sizeof(Record));
        memset((void *)new_record_ptr->name, '\0', sizeof(new_record_ptr->name));  // Enche todos os espaços vazios de '\0'
        strncpy(new_record_ptr->name, filename, sizeof(new_record_ptr->name) - 1); // Coloca o nome sobre os '\0'
        new_record_ptr->TypeVal = TYPEVAL_REGULAR;
        new_record_ptr->inodeNumber = new_inode_id;

        if (set_i_th_record_ptr_on_root_dir_given_itself(new_record_id, new_record_ptr) != SUCCESS)
        {
            free(new_record_ptr);
            return ERROR;
        }

        free(new_record_ptr);

        return SUCCESS;
    }
    else
    {
        delete2(filename);
        return ghost_create2(filename);
    }
}

int update_inode_on_disk(int inode_number, iNode inode)
{
    WORD block_size = partitions[mounted_partition_index].super_block.blockSize;
    WORD super_block_size = partitions[mounted_partition_index].super_block.superblockSize;
    WORD inode_bitmap_size = partitions[mounted_partition_index].super_block.freeInodeBitmapSize;
    WORD data_block_bitmap_size = partitions[mounted_partition_index].super_block.freeBlocksBitmapSize;

    DWORD partition_boot_sector_ptr = partitions[mounted_partition_index].boot_sector;
    WORD inode_disk_area_ptr_offset_in_partition = block_size * (super_block_size + inode_bitmap_size + data_block_bitmap_size);
    WORD inode_block_ptr_offset_in_inode_disk_area = inode_number * sizeof(iNode) / SECTOR_SIZE;

    unsigned int inode_block_ptr = (unsigned int)partition_boot_sector_ptr +
                                   (unsigned int)inode_disk_area_ptr_offset_in_partition +
                                   (unsigned int)inode_block_ptr_offset_in_inode_disk_area;

    unsigned char sector_buffer[SECTOR_SIZE];
    int success = read_sector(inode_block_ptr, sector_buffer);
    if (success != 0)
        return ERROR;

    int inode_entry_offset_in_inode_block = (inode_number % (sizeof(iNode) / SECTOR_SIZE)) * sizeof(iNode);

    for (int i = 0; i < sizeof(iNode) / sizeof(DWORD); i++)
    {
        insert_DWORD_value_in_its_position_on_buffer(((DWORD *)inode)[i], inode_entry_offset_in_inode_block + i * sizeof(DWORD), sector_buffer);
    }

    if (write_sector(inode_block_ptr, sector_buffer) != SUCCESS)
        return ERROR;

    return SUCCESS;
}

DWORD get_data_block_ptr_given_data_block_number(DWORD data_block_number)
{
    WORD block_size = partitions[mounted_partition_index].super_block.blockSize;
    WORD super_block_size = partitions[mounted_partition_index].super_block.superblockSize;
    WORD inode_bitmap_size = partitions[mounted_partition_index].super_block.freeInodeBitmapSize;
    WORD data_block_bitmap_size = partitions[mounted_partition_index].super_block.freeBlocksBitmapSize;
    DWORD inode_disk_area_size = partitions[mounted_partition_index].number_of_inodes * sizeof(iNode) / SECTOR_SIZE;

    DWORD partition_boot_sector_ptr = partitions[mounted_partition_index].boot_sector;
    DWORD data_block_disk_area_ptr_offset_in_partition = (DWORD)block_size * ((DWORD)super_block_size +
                                                                              (DWORD)inode_bitmap_size +
                                                                              (DWORD)data_block_bitmap_size +
                                                                              inode_disk_area_size);
    DWORD data_block_ptr_offset_in_data_block_disk_area = data_block_number * (DWORD)block_size;

    DWORD data_block_ptr = partition_boot_sector_ptr +
                           data_block_disk_area_ptr_offset_in_partition +
                           data_block_ptr_offset_in_data_block_disk_area;

    return data_block_ptr;
}

DWORD get_data_block_number_given_data_block_ptr(DWORD data_block_ptr)
{
    WORD block_size = partitions[mounted_partition_index].super_block.blockSize;
    WORD super_block_size = partitions[mounted_partition_index].super_block.superblockSize;
    WORD inode_bitmap_size = partitions[mounted_partition_index].super_block.freeInodeBitmapSize;
    WORD data_block_bitmap_size = partitions[mounted_partition_index].super_block.freeBlocksBitmapSize;
    DWORD inode_disk_area_size = partitions[mounted_partition_index].number_of_inodes * sizeof(iNode) / SECTOR_SIZE;

    DWORD partition_boot_sector_ptr = partitions[mounted_partition_index].boot_sector;
    DWORD data_block_disk_area_ptr_offset_in_partition = (DWORD)block_size * ((DWORD)super_block_size +
                                                                              (DWORD)inode_bitmap_size +
                                                                              (DWORD)data_block_bitmap_size +
                                                                              inode_disk_area_size);

    DWORD data_block_ptr_offset_in_data_block_disk_area = data_block_ptr - data_block_disk_area_ptr_offset_in_partition - partition_boot_sector_ptr;
    DWORD data_block_number = data_block_ptr_offset_in_data_block_disk_area / (DWORD)block_size;

    return data_block_number;
}