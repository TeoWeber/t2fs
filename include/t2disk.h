#ifndef __T2DSIK___
#define __T2DSIK___

typedef int boolean;
#define false 0
#define true 1
typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned int DWORD;

// constante de de inodes
#define INVALID_PTR (DWORD)0
#define INVALID_INODE_PTR (DWORD)0

// constantes de arquivos
#define MAX_FILE_NAME_SIZE 255
#define MAX_OPEN_FILES 10
#define PTR_START_POSITION (DWORD)0
#define HANDLE_USED true
#define HANDLE_UNUSED false
#define INVALID_HANDLE (FILE2) - 1
#define INVALID_RECORD_PTR 0

// constantes de partições
#define MAX_PARTITIONS 4
#define PARTITION_FORMATTED true
#define PARTITION_UNFORMATTED false
#define NO_MOUNTED_PARTITION -1

// códigos de retorno
#define SUCCESS 0
#define ERROR -1

// tipos de arquivos
#define TYPEVAL_INVALIDO 0x00
#define TYPEVAL_REGULAR 0x01
#define TYPEVAL_LINK 0x02

typedef int FILE2;
typedef int DIR2;

typedef struct t2fs_superbloco SuperBlock;
typedef struct t2fs_record Record;
typedef struct t2fs_inode iNode;

#pragma pack(push, 1)

/** Superbloco  - 19/2 */
struct t2fs_superbloco
{
	char id[4];				   /** "T2FS" */
	WORD version;			   /** 0x7E32 */
	WORD superblockSize;	   /** 1 = Número de blocos ocupados pelo superbloco */
	WORD freeBlocksBitmapSize; /** Número de blocos do bitmap de blocos de dados */
	WORD freeInodeBitmapSize;  /** Número de blocos do bitmap de i-nodes */
	WORD inodeAreaSize;		   /** Número de blocos reservados para os i-nodes */
	WORD blockSize;			   /** Número de setores que formam um bloco */
	DWORD diskSize;			   /** Número total de blocos da partição */
	DWORD Checksum;			   /** Soma dos 5 primeiros inteiros de 32 bits do superbloco */
};

/** Registro de diretório (entrada de diretório) - 19/2 */
struct t2fs_record
{
	BYTE TypeVal;
	char name[51];
	DWORD Nao_usado[2];
	DWORD inodeNumber;
};

/** i-node - 19/2 */
struct t2fs_inode
{
	DWORD blocksFileSize;
	DWORD bytesFileSize;
	DWORD dataPtr[2];
	DWORD singleIndPtr;
	DWORD doubleIndPtr;
	DWORD RefCounter;
	DWORD reservado;
};

#pragma pack(pop)

#endif
