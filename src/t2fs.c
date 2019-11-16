#include "../include/t2fs.h"

int identify2(char *name, int size)
{
	initialize_file_system();

	char *grupo = "Astelio Jose Weber (283864)\nFrederico Schwartzhaupt (304244)\nJulia Violato (290185)"; // Define texto informativo a ser exibidio

	strncpy(name, grupo, size); // Transfere o texto informativo a ser exibido, para o atributo que será utilizado na exibição

	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente uma partição do disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2(int partition, int sectors_per_block)
{
	initialize_file_system();

	if (partition >= MAX_PARTITIONS) // Índice de partição a ser formatada é maior que o índice da última partição (ou seja, índice inválido)
		return ERROR;
	if (partition < 0) // Índice da partição a ser formatada é menor que o índice da primeira partição (ou seja, índice inválido)
		return ERROR;
	if (sectors_per_block > partitions[partition].size_in_sectors)
		return ERROR;
	if (sectors_per_block <= 0)
		return ERROR;
	if (partition == mounted_partition_index)
		unmount(partition);

	strcopy(partitions[partition].super_block.id, "T2FS");
	partitions[partition].super_block.version = (WORD)0x7E32;
	partitions[partition].super_block.superblockSize = (WORD)1;

	partitions[partition].super_block.blockSize = (WORD)sectors_per_block;
	partitions[partition].super_block.diskSize = (DWORD)(partitions[partition].size_in_sectors / sectors_per_block);

	partitions[partition].super_block.inodeAreaSize = (WORD)1 +
													  (WORD)((partitions[partition].super_block.diskSize - (DWORD)1) / (DWORD)10);
	partitions[partition].super_block.freeInodeBitmapSize = (WORD)1 +
															((partitions[partition].super_block.inodeAreaSize - (WORD)1) / (WORD)(8 * sizeof(iNode)));

	DWORD remainingBlocks = partitions[partition].super_block.diskSize -
							(DWORD)1 -
							partitions[partition].super_block.inodeAreaSize -
							partitions[partition].super_block.freeInodeBitmapSize;
	partitions[partition].super_block.freeBlocksBitmapSize = (WORD)1 +
															 (WORD)((remainingBlocks - (DWORD)1) / (DWORD)(8 * SECTOR_SIZE + 1));

	partitions[partition].super_block.Checksum = checksum(partition);

	char buffer[SECTOR_SIZE];
	memcpy(buffer, &partitions[partition].super_block, SECTOR_SIZE);

	if (write_sector(partitions[partition].boot_sector, buffer) != SUCCESS) // Escreve o superbloco armazenado no buffer, no primeiro setor da partição a ser formatada
		return ERROR;

	partitions[partition].is_formatted = PARTITION_FORMATTED; // Define a partição formatada como formatada
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Monta a partição indicada por "partition" no diretório raiz
-----------------------------------------------------------------------------*/
int mount(int partition)
{
	initialize_file_system();

	if (partition >= MAX_PARTITIONS) // Índice de partição a ser montada é maior que o índice da última partição (ou seja, índice inválido)
		return ERROR;
	if (partition < 0) // Índice da partição a ser montada é menor que o índice da primeira partição (ou seja, índice inválido)
		return ERROR;
	if (mounted_partition_index != NO_MOUNTED_PARTITION) // Já existe uma partição montada
		return ERROR;
	if (!partitions[partition].is_formatted) // A partição a ser montada não foi formatada
		return ERROR;

	mounted_partition_index = partition; // Define a partição a ser montada como a partição montada (ou seja, monta ela)
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Desmonta a partição atualmente montada, liberando o ponto de montagem.
-----------------------------------------------------------------------------*/
int unmount(void)
{
	initialize_file_system();

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return ERROR;

	for (int handle = 0; handle < MAX_OPEN_FILES; handle++)
	{
		open_files[handle].handle_used = FILE_HANDLE_UNUSED;
	}

	mounted_partition_index = NO_MOUNTED_PARTITION;
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um
		arquivo já existente, o mesmo terá seu conteúdo removido e
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2(char *filename)
{
	initialize_file_system();

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco.
-----------------------------------------------------------------------------*/
int delete2(char *filename)
{
	initialize_file_system();

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2(char *filename)
{
	initialize_file_system();

	// encontra um handle disponível para abrir o arquivo
	for (int handle = 0; handle < MAX_OPEN_FILES; handle++)
	{
		if (open_files[handle].handle_used != FILE_HANDLE_UNUSED)
		{
			/*
			open_file_inodes[handle] = INODE_NUMBER; // Armazena o número do inode do arquivo de nome filename no handle encontrado (ou seja, o seleciona)
			*/

			open_files[handle].current_pointer = POINTER_START_POSITION;
			return handle;
		}
	}
	return ERROR;
}

int close2(FILE2 handle)
{
	initialize_file_system();

	if (verify_file_handle(handle) == false)
		return ERROR;

	open_files[handle].handle_used = FILE_HANDLE_UNUSED; // Libera o handle do arquivo fechado
	return SUCCESS;
}

int read2(FILE2 handle, char *buffer, int size)
{
	initialize_file_system();

	// verifica se o arquivo está aberto
	if (verify_file_handle(handle) == false)
		return ERROR;

	OpenFile file = open_files[handle];
	FileRecord file_record = file.record;

	iNode file_inode;
	if (retrieve_inode(file_record.inodeNumber, &file_inode) == ERROR)
		return ERROR;

	// verifica eof
	if (file.current_pointer >= file_inode.bytesFileSize)
		return ERROR;

	// verifica se size > o restante do arquivo
	if (size > file_inode.bytesFileSize - file.current_pointer)
		size = file_inode.bytesFileSize - file.current_pointer;

	// lê conteúdo e atualiza handle do arquivo
	if (read_n_bytes_from_file(file.current_pointer, size, file_inode, buffer) == ERROR)
		return ERROR;

	file.current_pointer += size;
	open_files[handle] = file;

	return size;
}

int write2(FILE2 handle, char *buffer, int size)
{
	initialize_file_system();

	if (verify_file_handle(handle) == false)
		return ERROR;

	OpenFile file = open_files[handle];
	FileRecord file_record = file.record;

	iNode file_inode;
	if (retrieve_inode(file_record.inodeNumber, &file_inode) == ERROR)
		return ERROR;

	int bytes_written = write_n_bytes_to_file(file.current_pointer, size, file_inode, buffer);
	if (bytes_written == ERROR)
		return ERROR;

	file.current_pointer += bytes_written;
	open_files[handle] = file;

	return bytes_written;
}

DIR2 opendir2(char *pathname)
{
	initialize_file_system();

	DIR2 dir_handle = retrieve_free_dir_handle();
	if (dir_handle == INVALID_HANDLE)
		return ERROR;

	FileRecord dir_record;
	if (retrieve_dir_record(pathname, &dir_record == ERROR))
		return ERROR;

	if (dir_record.TypeVal != TYPEVAL_REGULAR)
		return ERROR;

	open_directories[dir_handle].record = dir_record;
	open_directories[dir_handle].current_pointer = POINTER_START_POSITION;

	return dir_handle;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2(DIR2 handle, DIRENT2 *dentry)
{
	initialize_file_system();

	return -1;
}

int closedir2(DIR2 handle)
{
	initialize_file_system();

	if (verify_dir_handle(handle) == false)
		return ERROR;

	open_directories[handle].record.TypeVal = TYPEVAL_INVALIDO;
	open_directories[handle].record.inodeNumber = INVALID_POINTER;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink)
-----------------------------------------------------------------------------*/
int sln2(char *linkname, char *filename)
{
	initialize_file_system();

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (hardlink)
-----------------------------------------------------------------------------*/
int hln2(char *linkname, char *filename)
{
	initialize_file_system();

	return -1;
}
