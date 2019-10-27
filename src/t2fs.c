
/**
*/
#include "t2fs.h"


// Arquivos abertos
#define MAX_OPEN_FILES 10

#define HANDLE_UNUSED 0
DWORD open_file_inodes[MAX_OPEN_FILES] = { HANDLE_UNUSED };

#define POINTER_START_POSITION 0
DWORD open_file_pointer_positions[MAX_OPEN_FILES];


// Partições
#define MAX_PARTITIONS 4

#define FORMATED_PARTITION true
#define UNFORMATED_PARTITION false
boolean is_the_partition_formated[MAX_PARTITIONS] = { UNFORMATED_PARTITION };

#define UNDEFINED_BOOT_SECTOR 0
DWORD partition_boot_sectors[MAX_PARTITIONS] = { UNDEFINED_BOOT_SECTOR };

#define NO_MOUNTED_PARTITION -1
int mounted_partiction = NO_MOUNTED_PARTITION;


// Flags de retorno
#define SUCCESS 0
#define ERROR -1


/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
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
	
	is_the_partition_formated[partition] = FORMATED_PARTITION; // Define a partição formatada como formatada
    return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Monta a partição indicada por "partition" no diretório raiz
-----------------------------------------------------------------------------*/
int mount(int partition)
{
	if (partition >= MAX_PARTITIONS) // Índice de partição a ser montada é maior que o índice da última partição (ou seja, índice inválido)
		return ERROR
	if (partition < 0) // Índice da partição a ser montada é menor que o índice da primeira partição (ou seja, índice inválido)
		return ERROR;
	if (mounted_partition != NO_MOUNTED_PARTITION) // Já existe uma partição montada
		return ERROR;
	if (!is_the_partition_formated[partition]) // A partição a ser montada não foi formatada
		return ERROR;
	
	mounted_partition = partition; // Define a partição a ser montada como a partição montada (ou seja, monta ela)
    return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Desmonta a partição atualmente montada, liberando o ponto de montagem.
-----------------------------------------------------------------------------*/
int unmount(void)
{
	if (mounted_partition == NO_MOUNTED_PARTITION) // Não existe uma partição montada
		return ERROR;
	
	mounted_partiction = NO_MOUNTED_PARTITION; // Define que não há mais partições montadas (ou seja, desmonta a partição que estava montada)
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
	for (int handle = 0; handle < MAX_OPEN_FILES; i++) // Itera a lista de inodes de arquivos abertos em busca de uma posição (handle) livre
	{
		if (open_file_inodes[handle] != HANDLE_UNUSED) // Encontrada posição (handle) da lista que não está sendo usada
		{
			/*
			open_file_inodes[handle] = INODE_NUMBER; // Armazena o número do inode do arquivo de nome filename no handle encontrado (ou seja, o seleciona)
			*/

			open_file_pointer_positions[handle] = POINTER_START_POSITION; // Define como inicial a posição do ponteiro de leitura e escrita corrente do handle selecionado
			
			return handle; // Retorna o handle selecionado
		}
	}
	return ERROR; // Não foi encontrado handle que não esteja sendo utilizado
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle)
{
	if (handle >= MAX_OPEN_FILES) // Handle do arquivo a ser fechado é maior que o maior handle (ou seja, handle inválido)
		return ERROR;
	if (handle < 0) // Handle do arquivo a ser fechado é menor que o menor handle (ou seja, handle inválido)
		return ERROR;
	if (open_file_inodes[handle] == HANDLE_UNUSED) // Handle do arquivo a ser fechado não está vinculado a um arquivo (ou seja, handle inválido)
		return ERROR;

	open_file_inodes[handle] = HANDLE_UNUSED; // Libera o handle do arquivo a ser fechado (ou seja, "fecha ele")
    return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink)
-----------------------------------------------------------------------------*/
int sln2 (char *linkname, char *filename)
{
    return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (hardlink)
-----------------------------------------------------------------------------*/
int hln2(char *linkname, char *filename)
{
    return -1;
}
