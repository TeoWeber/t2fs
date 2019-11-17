#include "../include/t2fs.h"

int identify2(char *name, int size)
{
	initialize_file_system();

	char *grupo = "Astelio Jose Weber (283864)\nFrederico Schwartzhaupt (304244)\nJulia Violato (290185)"; // Definimos o texto informativo a ser exibidio

	strncpy(name, grupo, size); // Transferimos o texto informativo a ser exibido, para o atributo que será utilizado na exibição

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

	if (partition == mounted_partition_index) // Partição a ser formatada está montada, devemos demontá-la antes então. (certo?)
		unmount(partition);

	if (fill_partition_structure(partition, sectors_per_block) != SUCCESS) // Preenchemos os novos dados variáveis da partição (que não são fixados pelo MBR)
		return ERROR;
		
	if (write_sector(partitions[partition].boot_sector, (unsigned char *)&partitions[partition].super_block) != SUCCESS) // Escrevemos o superbloco no primeiro setor da partição a ser formatada
		return ERROR;

	if (reset_bitmaps(partition) != SUCCESS) // Zeramos os bitmaps de dados e de inodes
		return ERROR;

	if (format_root_dir(partition) != SUCCESS) // Formatamos o inode do diretório raiz da partição
		return ERROR;

	partitions[partition].is_formatted = PARTITION_FORMATTED; // Definimos a partição formatada como formatada

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

	mounted_partition_index = partition; // Definimos a partição a ser montada como a partição montada (ou seja, monta ela)

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
		open_files[handle].handle_used = HANDLE_UNUSED;
	}

	is_the_root_dir_open = false;

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

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return ERROR;

	FILE2 handle;
	if ((handle = get_first_unused_handle()) == INVALID_HANDLE)
		return ERROR;

	Record *record_pointer;
	if (record_pointer = get_record_pointer_from_file_given_filename(filename) == INVALID_RECORD_POINTER)
		return ERROR;
	
	open_files[handle].record = *record_pointer;

	open_files[handle].current_pointer = POINTER_START_POSITION;

	open_files[handle].handle_used = HANDLE_USED; // Alocamos o handle do arquivo aberto

	return handle;
}

int close2(FILE2 handle)
{
	initialize_file_system();

	if (!is_a_handle_used(handle))
		return ERROR;

	open_files[handle].handle_used = HANDLE_UNUSED; // Liberamos o handle do arquivo fechado

	return SUCCESS;
}

int read2(FILE2 handle, char *buffer, int size)
{
	initialize_file_system();

	// verifica se o arquivo está aberto
	if (!is_a_handle_used(handle))
		return ERROR;

	OpenFile file = open_files[handle];
	Record record = file.record;

	iNode *inode;
	if ((inode = get_inode_pointer_given_inode_number(record.inodeNumber)) == INVALID_INODE_POINTER)
		return ERROR;

	// verifica eof
	if (file.current_pointer >= inode->bytesFileSize)
		return ERROR;

	// verifica se size > o restante do arquivo
	if (size > inode->bytesFileSize - file.current_pointer)
		size = inode->bytesFileSize - file.current_pointer;

	// lê conteúdo e atualiza handle do arquivo
	if (read_n_bytes_from_file(file.current_pointer, size, *inode, buffer) == ERROR)
		return ERROR;

	file.current_pointer += size;
	open_files[handle] = file;

	return size;
}

int write2(FILE2 handle, char *buffer, int size)
{
	initialize_file_system();

	if (!is_a_handle_used(handle))
		return ERROR;

	OpenFile file = open_files[handle];
	Record record = file.record;

	iNode *inode;
	if ((inode = get_inode_pointer_given_inode_number(record.inodeNumber)) == INVALID_INODE_POINTER)
		return ERROR;

	int bytes_written = write_n_bytes_to_file(file.current_pointer, size, *inode, buffer);
	if (bytes_written == ERROR)
		return ERROR;

	file.current_pointer += bytes_written;
	open_files[handle] = file;

	return bytes_written;
}

int opendir2(void)
{
	initialize_file_system();
	
	if (is_the_root_dir_open)
		return ERROR;

	is_the_root_dir_open = true;
	root_dir_entry_current_pointer = 0;

	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2(DIRENT2 *dentry)
{
	initialize_file_system();

	if (!is_the_root_dir_open)
		return ERROR;

	// COLOCAR EM dentry OS DADOS DO i-ÉSIMO FILE RECORD DA RAIZ
	// PRIMEIRO FILE RECORD ESTÁ EM i = 0
	// i = root_dir_entry_current_pointer

	root_dir_entry_current_pointer++;

	return SUCCESS;
}

int closedir2(void)
{
	initialize_file_system();

	if (!is_the_root_dir_open)
		return ERROR;

	is_the_root_dir_open = false;

	return SUCCESS;
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
