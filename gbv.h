#ifndef GBV_H
#define GBV_H

#include <time.h>

#define MAX_NAME 256
#define BUFFER_SIZE 512   // tamanho fixo do buffer em bytes

// Tam maximo para guardar nome do arquivo
#define MAX_ARCHIVE_PATH 512

// Estrutura de metadados de cada documento
typedef struct {
    char name[MAX_NAME];   // nome do documento
    long size;             // tamanho em bytes
    time_t date;           // data de inserção
    long offset;           // posição no container
} Document;

// Estrutura que representa a biblioteca (diretório em memória)
typedef struct {
    Document *docs;        // vetor dinâmico de documentos
    int count;             // número de documentos
} Library;

// Estrutura para representar o superbloco
typedef struct {
	int count;
	long dir_offset;
} GBV_Superblock;

// Funções que voce deve implementar em gbv.c
int gbv_create(const char *filename);
int gbv_open(Library *lib, const char *filename);
int gbv_add(Library *lib, const char *archive, const char *docname);
int gbv_remove(Library *lib, const char *docname);
int gbv_list(const Library *lib);
int gbv_view(const Library *lib, const char *docname);
int gbv_order(Library *lib, const char *archive, const char *criteria);

//Funcao auxiliar para liberar a memoria                                                                               
void gbv_close (Library *lib); //verificar se podemos fazer isso 

#endif

