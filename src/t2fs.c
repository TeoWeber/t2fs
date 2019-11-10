#include "t2fs.h"


int identify2 (char *name, int size)
{
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

	char buffer[SECTOR_SIZE]; // Buffer para armazenar conteúdo (setor) a ser usado em leituras e escritas de setores

	if (partition_boot_sectors[partition] == UNDEFINED_BOOT_SECTOR) // A partição ainda não teve o seu endereço inicial guardado
	{
		if (read_sector(0, buffer) != SUCCESS) // Falha ao ler o MBR
			return ERROR;

		partition_boot_sectors[partition] = // Guarda o endereço do primeiro setor da partição a ser formatada
		 (uint)buffer[8 + 32 * partition + 3] >> 8*3 &
		  (uint)buffer[8 + 32 * partition + 2] >> 8*2 &
		   (uint)buffer[8 + 32 * partition + 1] >> 8*1 &
		    (uint)buffer[8 + 32 * partition + 0] >> 8*0;
	}

	/* 
	buffer = SUPERBLOCO; // Armazena no buffer o superbloco a ser escrito no primeiro setor da partição a ser formata
	*/

	if (read_sector(partition_boot_sectors[partition], buffer) != SUCCESS) // Escreve o superbloco armazenado no primeiro setor da partição a ser formata (ou seja, formata ela)
		return ERROR;
	
	is_partition_formatted[partition] = PARTITION_FORMATTED; // Define a partição formatada como formatada
    return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Monta a partição indicada por "partition" no diretório raiz
-----------------------------------------------------------------------------*/
int mount(int partition)
{
	if ( partition >= MAX_PARTITIONS)  // Índice de partição a ser montada é maior que o índice da última partição (ou seja, índice inválido)
		return ERROR;
	if ( partition < 0 ) // Índice da partição a ser montada é menor que o índice da primeira partição (ou seja, índice inválido)
		return ERROR;
	if ( mounted_partition != NO_MOUNTED_PARTITION ) // Já existe uma partição montada
		return ERROR;
	if ( !is_partition_formatted[partition] ) // A partição a ser montada não foi formatada
		return ERROR;
	
	mounted_partition = partition; // Define a partição a ser montada como a partição montada (ou seja, monta ela)
    return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Desmonta a partição atualmente montada, liberando o ponto de montagem.
-----------------------------------------------------------------------------*/
int unmount(void)
{
	if ( mounted_partition == NO_MOUNTED_PARTITION )
		return ERROR;
	
	mounted_partition = NO_MOUNTED_PARTITION;
    return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um
		arquivo já existente, o mesmo terá seu conteúdo removido e
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco.
-----------------------------------------------------------------------------*/
int delete2 (char *filename)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename)
{
	// encontra um handle disponível para abrir o arquivo
	for ( int handle = 0; handle < MAX_OPEN_FILES; handle++ ) 
	{
		if ( open_file_inodes[handle] != FILE_HANDLE_UNUSED )
		{
			/*
			open_file_inodes[handle] = INODE_NUMBER; // Armazena o número do inode do arquivo de nome filename no handle encontrado (ou seja, o seleciona)
			*/

			open_file_pointer_positions[handle] = POINTER_START_POSITION;
			return handle;
		}
	}
	return ERROR;
}

int close2 (FILE2 handle)
{
	if ( verify_file_handle( handle ) == false )
		return ERROR;

	open_file_inodes[handle] = FILE_HANDLE_UNUSED;  // Libera o handle do arquivo fechado
    return SUCCESS;
}

int read2 (FILE2 handle, char *buffer, int size)
{
	initialize_file_system();

	// verifica se o arquivo está aberto
	if ( verify_file_handle( handle ) == false )
		return ERROR;

	OpenFile file = open_files[handle];
	FileRecord file_record = file.record;

	iNode file_inode;
	if ( retrieve_inode( file_record.inodeNumber, &file_inode ) == ERROR )
		return ERROR;

	// verifica eof
	if ( file.current_pointer >= file_inode.bytesFileSize )
		return ERROR;

	// verifica se size > o restante do arquivo
	if ( size > file_inode.bytesFileSize - file.current_pointer )
		size = file_inode.bytesFileSize - file.current_pointer;

	// lê conteúdo e atualiza handle do arquivo
	if ( read_n_bytes_from_file( file.current_pointer, size, file_inode, buffer ) == ERROR )
		return ERROR;

	file.current_pointer += size;
	open_files[handle] = file;

	return size;
}

int write2 (FILE2 handle, char *buffer, int size)
{
	initialize_file_system();

	if ( verify_file_handle( handle ) == false )
		return ERROR;

	OpenFile file = open_files[handle];
	FileRecord file_record = file.record;

	iNode file_inode;
	if ( retrieve_inode( file_record.inodeNumber, &file_inode ) == ERROR )
		return ERROR;

	int bytes_written = write_n_bytes_to_file( file.current_pointer, size, file_inode, buffer );
	if ( bytes_written == ERROR )
		return ERROR;

    file.current_pointer += bytes_written;
	open_files[handle] = file;
	
	return bytes_written;
}

DIR2 opendir2 (char *pathname)
{
	initialize_file_system();

	DIR2 dir_handle = retrieve_free_dir_handle();
	if ( dir_handle == INVALID_HANDLE )
		return ERROR;

    FileRecord dir_record;
	if ( retrieve_dir_record( pathname, &dir_record == ERROR ) )
		return ERROR;

	if ( dir_record.TypeVal != TYPEVAL_REGULAR )
		return ERROR;

	open_directories[dir_handle].record = dir_record;
	open_directories[dir_handle].current_pointer = POINTER_START_POSITION;

	return dir_handle;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry)
{
	initialize_file_system();

    return -1;
}

int closedir2 (DIR2 handle)
{
	initialize_file_system();

	if ( verify_dir_handle( handle ) == false )
		return ERROR;
	
    open_directories[handle].record.TypeVal = TYPEVAL_INVALIDO;
	open_directories[handle].record.inodeNumber = INVALID_POINTER;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink)
-----------------------------------------------------------------------------*/
int sln2 (char *linkname, char *filename)
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
