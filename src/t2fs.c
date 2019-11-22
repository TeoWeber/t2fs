#include "t2fs.h"

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

	if (partition == mounted_partition_index)
		umount(partition);

	if (reset_partition_sectors(partition) != SUCCESS)
		return ERROR;

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
int umount(void)
{
	initialize_file_system();

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return ERROR;

	int handle;
	for (handle = 0; handle < MAX_OPEN_FILES; handle++)
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

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return INVALID_HANDLE;

	FILE2 handle;
	if ((handle = get_first_unused_handle()) == INVALID_HANDLE)
		return INVALID_HANDLE;

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

		open_files[handle].record = *new_record_ptr;
		open_files[handle].current_ptr = PTR_START_POSITION;
		open_files[handle].handle_used = HANDLE_USED;

	}
	else
	{
		delete2(filename);
		return create2(filename);
	}

	return handle;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco.
-----------------------------------------------------------------------------*/
int delete2(char *filename)
{
	initialize_file_system();

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return ERROR;

	Record *record_ptr;
	record_ptr = get_record_ptr_from_file_given_filename(filename);
	if (record_ptr == INVALID_RECORD_PTR)
		return ERROR;

	if (record_ptr->TypeVal == TYPEVAL_INVALIDO)
		return ERROR;

	record_ptr->TypeVal = TYPEVAL_INVALIDO;
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2(char *filename)
{
	initialize_file_system();

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return INVALID_HANDLE;

	FILE2 handle;
	if ((handle = get_first_unused_handle()) == INVALID_HANDLE)
		return INVALID_HANDLE;

	Record *record_ptr;
	record_ptr = get_record_ptr_from_file_given_filename(filename);
	if (record_ptr == INVALID_RECORD_PTR)
		return INVALID_HANDLE;

	if (record_ptr->TypeVal == TYPEVAL_INVALIDO)
		return ERROR;

	open_files[handle].record = *record_ptr;

	open_files[handle].current_ptr = PTR_START_POSITION;

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

	iNode *inode_ptr;
	if ((inode_ptr = get_inode_ptr_given_inode_number(record.inodeNumber)) == INVALID_INODE_PTR)
		return ERROR;

	// verifica eof
	if (file.current_ptr >= inode_ptr->bytesFileSize)
		return ERROR;

	// verifica se size > o restante do arquivo
	if (size > (int)(inode_ptr->bytesFileSize - file.current_ptr))
		size = inode_ptr->bytesFileSize - file.current_ptr;

	// lê conteúdo e atualiza handle do arquivo
	if (read_n_bytes_from_file(file.current_ptr, size, *inode_ptr, buffer) == ERROR)
		return ERROR;

	file.current_ptr += size;
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

	int bytes_written = write_n_bytes_to_file(file.current_ptr, size, record.inodeNumber, buffer);
	if (bytes_written == ERROR)
		return ERROR;

	file.current_ptr += bytes_written;
	open_files[handle] = file;

	return bytes_written;
}

int opendir2(void)
{
	initialize_file_system();

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return ERROR;

	if (is_the_root_dir_open)
		return ERROR;

	is_the_root_dir_open = true;
	root_dir_entry_current_ptr = PTR_START_POSITION;

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

	Record *record_ptr;
	record_ptr = get_i_th_record_ptr_from_root_dir(root_dir_entry_current_ptr);
	if (!is_used_record_ptr(record_ptr))
		return ERROR;

	iNode *inode_ptr;
	if ((inode_ptr = get_inode_ptr_given_inode_number(record_ptr->inodeNumber)) == INVALID_INODE_PTR)
		return ERROR;

	strcpy(dentry->name, record_ptr->name);
	dentry->fileType = record_ptr->TypeVal;
	dentry->fileSize = inode_ptr->bytesFileSize;

	root_dir_entry_current_ptr++;

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

	// Se não existir partição montada: erro.
	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return ERROR;

	// Verifica se existe o arquivo que o softlink vai referenciar. Caso não exista: erro.
	Record *ref_record_ptr;
	ref_record_ptr = get_record_ptr_from_file_given_filename(filename);
	if (ref_record_ptr == INVALID_RECORD_PTR)
		return ERROR;

	// Se o arquivo foi deletado: erro.
	if (ref_record_ptr->TypeVal == TYPEVAL_INVALIDO)
		return ERROR;

	// Criamos o arquivo de link então. Se falhar: erro.
	if (ghost_create2(linkname) != SUCCESS)
		return ERROR;

	// Puxamos o ponteiro pro registro do arquivo criado pela deep web, já que a create não devolve ele. Se não achar: uehh, erro.
	Record *link_record_ptr;
	link_record_ptr = get_record_ptr_from_file_given_filename(linkname);
	if (link_record_ptr == INVALID_RECORD_PTR)
		return ERROR;

	// Definimos o tipo do registro como soflink.
	link_record_ptr->TypeVal = TYPEVAL_LINK;

	// Pegamos o inode, sabendo o número dele. Isso se não der erro.
	iNode *link_inode_ptr;
	link_inode_ptr = get_inode_ptr_given_inode_number(link_record_ptr->inodeNumber);
	if (link_inode_ptr == INVALID_INODE_PTR)
		return ERROR;

	int bytes_written = write_n_bytes_to_file(0, strlen(filename), link_record_ptr->inodeNumber, filename);
	if (bytes_written == ERROR)
		return ERROR;

	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (hardlink)
-----------------------------------------------------------------------------*/
int hln2(char *linkname, char *filename)
{
	initialize_file_system();

	if (mounted_partition_index == NO_MOUNTED_PARTITION)
		return ERROR;

	Record *ref_record_ptr;
	ref_record_ptr = get_record_ptr_from_file_given_filename(filename);
	if (ref_record_ptr == INVALID_RECORD_PTR)
		return ERROR;

	if (ref_record_ptr->TypeVal = TYPEVAL_INVALIDO)
		return ERROR;

	if (ghost_create2(linkname) != SUCCESS)
		return ERROR;

	Record *link_record_ptr;
	link_record_ptr = get_record_ptr_from_file_given_filename(linkname);
	if (link_record_ptr == INVALID_RECORD_PTR)
		return ERROR;

	link_record_ptr->TypeVal = ref_record_ptr->TypeVal;
	link_record_ptr->inodeNumber = ref_record_ptr->inodeNumber;

	iNode *inode_ptr;
	if ((inode_ptr = get_inode_ptr_given_inode_number(link_record_ptr->inodeNumber)) == INVALID_INODE_PTR)
		return ERROR;

	inode_ptr->RefCounter++;

	return SUCCESS;
}
