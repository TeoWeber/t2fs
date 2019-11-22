#include "support.h"

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
    memset(emptyBuffer, '\0', SECTOR_SIZE);
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

        if (!is_used_record_ptr(record_ptr))
            return INVALID_RECORD_PTR;

        if (strcmp(record_ptr->name, filename) == 0) // Se as strings são iguais strcmp retorna 0
            break;
    }

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

DWORD get_i_from_first_invalid_record()
{
    // Percorremos todas entradas em busca de uma entrada com nome == filename
    boolean hitFlag = false;
    Record *record_ptr;
    int i;

    for (i = 0; 1; i++)
    {
        record_ptr = get_i_th_record_ptr_from_root_dir(i);

        if (!is_used_record_ptr(record_ptr))
            break;
    }

    return (DWORD)i;
}

DWORD get_free_inode_number_in_partition()
{
    DWORD inode;
    openBitmap2(partitions[mounted_partition_index].boot_sector);
    inode = searchBitmap2(BITMAP_INODE, 0); // Procura por uma posição vazia no Bitmap e retorna sua posição

    if (inode <= 0)
    { // Sem espaço livre!
        printf("There's no space in this partition.");
        closeBitmap2();
        return ERROR;
    }

    setBitmap2(BITMAP_INODE, inode, 1); // Ativa este inode no Bitmap da partição

    closeBitmap2();
    return inode;
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
    iNode *inode;
    inode = get_inode_ptr_given_inode_number(inode_number);
    if (inode == INVALID_INODE_PTR)
        return INVALID_INODE_PTR;

    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int ptr_per_block = block_size * 64;

    if (i == 1)
        return inode->dataPtr[0];
    else if (i == 2)
        return inode->dataPtr[1];
    else if (i <= ptr_per_block + 2)
    {
        DWORD ptrs[ptr_per_block];
        retrieve_ptrs_from_block(inode->singleIndPtr, ptrs);

        return ptrs[i-3];
    }
    else if (i > ptr_per_block + 2)
    {
        DWORD ind_ptrs[ptr_per_block];
        retrieve_ptrs_from_block(inode->doubleIndPtr, ind_ptrs);
        
        int ptr_block_pos;
        i -= (ptr_per_block + 2);
        ptr_block_pos = i / ptr_per_block;

        DWORD ptrs[ptr_per_block];
        retrieve_ptrs_from_block(ind_ptrs[ptr_block_pos], ptrs);

        int block_pos = i % ptr_per_block;
        return ptrs[block_pos];
    }

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

    if (getBitmap2(BITMAP_INODE, inode_number) == 0)
        return INVALID_INODE_PTR;

    unsigned char sector_buffer[SECTOR_SIZE];
    if (read_sector(sector, sector_buffer) != 0)
        return INVALID_INODE_PTR;

    iNode *inode_ptr;
    inode_start = (inode_number % 8) * 32;
    inode_ptr->blocksFileSize = *((DWORD*) (sector_buffer + inode_start + 0));
    inode_ptr->bytesFileSize = *((DWORD*) (sector_buffer + inode_start + 4));
    inode_ptr->dataPtr[0] = *((DWORD*) (sector_buffer + inode_start + 8));
    inode_ptr->dataPtr[1] = *((DWORD*) (sector_buffer + inode_start + 12));
    inode_ptr->singleIndPtr = *((DWORD*) (sector_buffer + inode_start + 16));
    inode_ptr->doubleIndPtr = *((DWORD*) (sector_buffer + inode_start + 20));
    inode_ptr->RefCounter = *((DWORD*) (sector_buffer + inode_start + 24));
    inode_ptr->reservado = *((DWORD*) (sector_buffer + inode_start + 28));

    return inode_ptr;
}

void retrieve_ptrs_from_block(DWORD block_number, DWORD *ptrs)
{
    unsigned char sector_buffer[SECTOR_SIZE];
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int ptr_per_sector = block_size * 4;

    int i;
    for (i = 0; i < block_size; i++)
    {
        int sector = i + block_number * block_size;
        read_sector(sector, sector_buffer);

        int j;
        for (j = 0; j < ptr_per_sector; j++)
            ptrs[j + i * ptr_per_sector] = *((DWORD *)(sector_buffer + j * 4));
    }
}

int read_block_from_block_number(int ptr, int block_number, int bytes, char *buffer)
{
    int read_sectors = 0;
    int read_bytes = 0;
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int sector = block_number * block_size + (ptr / SECTOR_SIZE);

    unsigned char block_buffer[SECTOR_SIZE];
    while (read_sectors < block_size)
    {
        int success = read_sector(sector, block_buffer);
        if (success == 0)
        {
            int first = ptr - read_sectors * SECTOR_SIZE;
            int i;
            for (i = first; i < SECTOR_SIZE; i++)
            {
                if (read_bytes < bytes) // ainda nao leu todos
                {
                    buffer[read_bytes] = block_buffer[i];
                    read_bytes += 1;
                    ptr += 1;
                }
            }
            sector = block_number * block_size + (ptr / SECTOR_SIZE);
        }
        read_sectors += 1;
    }

    return read_bytes;
}

int read_n_bytes_from_file(DWORD ptr, int n, iNode inode, char *buffer)
{
    int read_bytes = 0, remaining_bytes = n;
    int curr_byte = 0;
    int sector;
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    char aux_buff[block_size * SECTOR_SIZE];

    int curr_block, curr_ptr;
    int ptr_per_block = block_size * 64;
    curr_block = 1 + ptr / (block_size * SECTOR_SIZE);
    curr_ptr = ptr - (curr_block - 1) * (block_size * SECTOR_SIZE);

    // ponteiros diretos
    if (curr_block == 1)
    {
        read_bytes = read_block_from_block_number(curr_ptr, inode.dataPtr[curr_block - 1], n, aux_buff);
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
        curr_ptr = ptr - (curr_block - 1) * (block_size * SECTOR_SIZE);
        read_bytes = read_block_from_block_number(curr_ptr, inode.dataPtr[curr_block - 1], n, aux_buff);
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
        retrieve_ptrs_from_block(inode.singleIndPtr, ptrs);

        int i;
        for (i = curr_block - 3; i < ptr_per_block; i++)
        {
            curr_ptr = ptr - (curr_block - 1) * (block_size * SECTOR_SIZE);
            read_bytes = read_block_from_block_number(curr_ptr, inode.dataPtr[curr_block - 1], n, aux_buff);
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
            curr_block = 1 + ptr / (block_size * SECTOR_SIZE);
        }
    }

    if (remaining_bytes <= 0)
        return read_bytes;

    // ponteiro de indireção dupla
    if (curr_block > ptr_per_block + 2)
    {
        DWORD ptrs[ptr_per_block], ind_ptrs[ptr_per_block];
        retrieve_ptrs_from_block(inode.doubleIndPtr, ind_ptrs);

        int first = (curr_block - ptr_per_block - 3) / ptr_per_block;
        int i;
        for (i = first; i < ptr_per_block && read_bytes < n; i++)
        {
            retrieve_ptrs_from_block(curr_block + i, ptrs);

            first = (curr_block - ptr_per_block - i - 3) - i * ptr_per_block;
            int j;
            for (j = first; j < ptr_per_block && read_bytes < n; j++)
            {
                curr_ptr = ptr - (curr_block - 1) * (block_size * SECTOR_SIZE);
                read_bytes = read_block_from_block_number(curr_ptr, ptrs[j], n, aux_buff);

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
                        curr_block = 1 + ptr / (block_size * SECTOR_SIZE);
                }
            }
        }
    }

    // falta: caso em que é um softlink

    return read_bytes;
}

void write_block_to_block_number(DWORD block_number, char *buffer)
{
    unsigned char sector_buffer[SECTOR_SIZE];
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int sector = block_number * block_size;
    int written_sectors = 0, written_bytes = 0;

    while (written_sectors < block_size)
    {
        // copia o buffer para escrever no setor
        int i;
        for (i = 0; i < SECTOR_SIZE; i++)
        {
            sector_buffer[i] = buffer[written_bytes];
            written_bytes += 1;
        }

        int success = write_sector(sector, sector_buffer);
        if (success == 0)
        {
            sector += 1;
            written_sectors += 1;
        }
    }
}

int initialize_new_ptr_block()
{
    DWORD new_ptr = INVALID_PTR;
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    unsigned char block_buffer[SECTOR_SIZE];

    int i;
    for (i = 0; i < 64; i++)
        insert_ptr_in_buffer(new_ptr, (i % 64) * 4, block_buffer);

    int new_block_number = searchBitmap2(BITMAP_DADOS, 0);
    if (new_block_number <= 0)
        return ERROR;

    for (i = 0; i < block_size; i++)
    {
        int success = write_sector(new_block_number * block_size + i, block_buffer);
        if (success != 0)
            return ERROR;
    }

    setBitmap2(BITMAP_DADOS, new_block_number, 1);
    return new_block_number;
}

void insert_ptr_in_buffer(DWORD ptr, int starting_pos, unsigned char *block_buffer)
{
    unsigned char *aux_ptr = (unsigned char *)&ptr;

    block_buffer[starting_pos] = aux_ptr[0];
    block_buffer[starting_pos + 1] = aux_ptr[1];
    block_buffer[starting_pos + 2] = aux_ptr[2];
    block_buffer[starting_pos + 3] = aux_ptr[3];
}

int update_inode_on_disk(int inode_number, iNode inode)
{
    SuperBlock super_block = partitions[mounted_partition_index].super_block;
    int block_size = super_block.blockSize;
    int super_block_size = super_block.superblockSize;
    int block_bitmap_size = super_block.freeBlocksBitmapSize;
    int inode_bitmap_size = super_block.freeInodeBitmapSize;

    int inode_start;
    int inode_disk_area = block_size * (super_block_size + block_bitmap_size + inode_bitmap_size);
    int sector = inode_disk_area + inode_number / 8;

    unsigned char sector_buffer[SECTOR_SIZE];
    int success = read_sector(sector, sector_buffer);
    if (success != 0)
        return ERROR;

    inode_start = (inode_number % 8) * 32;

    insert_ptr_in_buffer(inode.blocksFileSize, inode_start, sector_buffer);
    insert_ptr_in_buffer(inode.bytesFileSize, inode_start + 4, sector_buffer);
    insert_ptr_in_buffer(inode.dataPtr[0], inode_start + 8, sector_buffer);
    insert_ptr_in_buffer(inode.dataPtr[1], inode_start + 12, sector_buffer);
    insert_ptr_in_buffer(inode.singleIndPtr, inode_start + 16, sector_buffer);
    insert_ptr_in_buffer(inode.doubleIndPtr, inode_start + 20, sector_buffer);

    success = write_sector(sector, sector_buffer);
    if (success != 0)
        return ERROR;

    return SUCCESS;
}

int write_new_ptr_to_block(int i, DWORD block_number, DWORD ptr)
{
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int sector = block_number * block_size + (i * 4) / SECTOR_SIZE;

    unsigned char block_buffer[SECTOR_SIZE];

    int success = read_sector(sector, block_buffer);
    if (success != 0)
        return ERROR;

    insert_ptr_in_buffer(ptr, (i % 64) * 4, block_buffer);

    success = write_sector(sector, block_buffer);
    if (success != 0)
        return ERROR;

    return SUCCESS;
}

int write_n_bytes_to_file(DWORD ptr, int n, int inodenum, char *buffer)
{
    iNode *inode;
	if ((inode = get_inode_ptr_given_inode_number(inodenum)) == INVALID_INODE_PTR)
		return ERROR;

    int written_bytes = 0, remaining_bytes = n;
    int sector;
    int block_size = partitions[mounted_partition_index].super_block.blockSize;
    int ptr_per_block = block_size * 64;

    char aux_buff[block_size * SECTOR_SIZE];

    int curr_block, curr_ptr;
    DWORD first_ptr = ptr;

    curr_block = 1 + ptr / (block_size * SECTOR_SIZE);
    // ponteiros diretos
    if (curr_block == 1)
    {
        curr_ptr = ptr - (curr_block - 1) * block_size * SECTOR_SIZE;
        if (inode->dataPtr[0] == INVALID_PTR)
        {
            inode->dataPtr[0] = searchBitmap2(BITMAP_DADOS, 0);
            if (inode->dataPtr[0] < 0)
                return ERROR;

            setBitmap2(BITMAP_DADOS, inode->dataPtr[0], 1);
            inode->blocksFileSize += 1;
        }

        int i;
        for (i = curr_ptr; i < block_size * SECTOR_SIZE && remaining_bytes > 0; i++)
        {
            aux_buff[i] = buffer[written_bytes];
            written_bytes += 1;
            remaining_bytes -= 1;
        }

        write_block_to_block_number(inode->dataPtr[0], aux_buff);
        if (remaining_bytes > 0)
            curr_block += 1;
        ptr += written_bytes;
    }

    if (curr_block == 2)
    {
        curr_ptr = ptr - (curr_block - 1) * block_size * SECTOR_SIZE;
        if (inode->dataPtr[1] == INVALID_PTR)
        {
            inode->dataPtr[1] = searchBitmap2(BITMAP_DADOS, 0);
            if (inode->dataPtr[1] < 0)
                return ERROR;

            setBitmap2(BITMAP_DADOS, inode->dataPtr[1], 1);
            inode->blocksFileSize += 1;
        }

        int i;
        for (i = curr_ptr; i < block_size * SECTOR_SIZE && remaining_bytes > 0; i++)
        {
            aux_buff[i] = buffer[written_bytes];
            written_bytes += 1;
            remaining_bytes -= 1;
        }

        write_block_to_block_number(inode->dataPtr[1], aux_buff);
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
            DWORD new_ptr = initialize_new_ptr_block();
            if (new_ptr < 0)
                return ERROR;
            inode->singleIndPtr = new_ptr;
        }

        DWORD ptrs[ptr_per_block];
        retrieve_ptrs_from_block(inode->singleIndPtr, ptrs);

        int i;
        for (i = curr_block - 3; i < ptr_per_block; i++)
        {
            if (remaining_bytes > 0)
            {
                curr_ptr = ptr - (curr_block - 1) * block_size * SECTOR_SIZE;
                if (ptrs[i] == INVALID_PTR)
                {
                    ptrs[i] = searchBitmap2(BITMAP_DADOS, 0);
                    if (ptrs[i] < 0)
                        return ERROR;

                    setBitmap2(BITMAP_DADOS, ptrs[i], 1);
                    inode->blocksFileSize += 1;

                    write_new_ptr_to_block(i, inode->singleIndPtr, ptrs[i]);
                }

                int j;
                for (j = curr_ptr; j < block_size * SECTOR_SIZE && remaining_bytes > 0; j++)
                {
                    aux_buff[j] = buffer[written_bytes];
                    written_bytes += 1;
                    remaining_bytes -= 1;
                }

                write_block_to_block_number(ptrs[i], aux_buff);
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
            DWORD new_ptr = initialize_new_ptr_block();
            if (new_ptr < 0)
                return ERROR;
            inode->doubleIndPtr = new_ptr;
        }

        DWORD ind_ptrs[ptr_per_block];
        retrieve_ptrs_from_block(inode->doubleIndPtr, ind_ptrs);

        int first = (curr_block - ptr_per_block - 3) / ptr_per_block;
        int i;
        for (i = first; i < ptr_per_block; i++)
        {
            if (remaining_bytes > 0)
            {
                if (ind_ptrs[i] == INVALID_PTR)
                {
                    DWORD new_ptr = initialize_new_ptr_block();
                    if (new_ptr < 0)
                        return ERROR;
                    ind_ptrs[i] = new_ptr;

                    write_new_ptr_to_block(i, inode->doubleIndPtr, ind_ptrs[i]);
                }

                DWORD ptrs[ptr_per_block];
                retrieve_ptrs_from_block(ind_ptrs[i], ptrs);

                first = (curr_block - ptr_per_block - 3) - i * ptr_per_block;
                int j;
                for (j = first; j < ptr_per_block; j++)
                {
                    if (remaining_bytes > 0)
                    {
                        curr_ptr = ptr - (curr_block - 1) * block_size * SECTOR_SIZE;
                        if (ptrs[j] == INVALID_PTR)
                        {
                            ptrs[j] = searchBitmap2(BITMAP_DADOS, 0);
                            if (ptrs[j] < 0)
                                return ERROR;

                            setBitmap2(BITMAP_DADOS, ptrs[j], 1);
                            inode->blocksFileSize += 1;

                            write_new_ptr_to_block(j, ind_ptrs[i], ptrs[j]);
                        }

                        int k;
                        for (k = curr_ptr; k < block_size * SECTOR_SIZE && remaining_bytes > 0; k++)
                        {
                            aux_buff[k] = buffer[written_bytes];
                            written_bytes += 1;
                            remaining_bytes -= 1;
                        }

                        write_block_to_block_number(ptrs[i], aux_buff);
                        if (remaining_bytes > 0)
                            curr_block += 1;

                        curr_ptr = first_ptr + written_bytes;
                    }
                }
            }
        }
    }

    if (curr_ptr > inode->bytesFileSize)
        inode->bytesFileSize = curr_ptr;
    update_inode_on_disk(inodenum, *inode);

    return written_bytes;
}

boolean is_used_record_ptr(Record *record_ptr)
{
    char emptyRecord[sizeof(Record)];
    memset(emptyRecord, '\0', sizeof(Record));

    if (strcmp((char *) record_ptr, (char *) emptyRecord) == 0)
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

		iNode new_inode;
		define_empty_inode_from_inode_ptr(&new_inode);
		new_inode.RefCounter = 1;

		update_inode_on_disk(new_inode_id, new_inode);

		DWORD new_record_id = get_i_from_first_invalid_record();

		Record *new_record_ptr = get_i_th_record_ptr_from_root_dir(new_record_id);
		memset(new_record_ptr->name, '\0', sizeof(new_record_ptr->name));		   // Enche todos os espaços vazios de '\0'
		strncpy(new_record_ptr->name, filename, sizeof(new_record_ptr->name) - 1); // Coloca o nome sobre os '\0'
		new_record_ptr->TypeVal = TYPEVAL_REGULAR;
		new_record_ptr->inodeNumber = new_inode_id;
	}
	else
	{
		delete2(filename);
		ghost_create2(filename);
	}

	return SUCCESS;
}
