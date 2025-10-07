#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gbv.h"
#include "util.h"

//----------------------------------------------------------------------------------------//
// DECLARACOES GLOBAIS E DE FUNCOES (AUXILIARES)
//----------------------------------------------------------------------------------------//

// Varial global para previnir alguns erros (pode ser removida)
Document *gbv;

// Variavel global para definir o nome do arquivo
// Facilita o acesso ao nome do arquivo em diferentes funcoes sem passa-lo como parametro
static char GBV_ARCHIVE_NAME[MAX_ARCHIVE_PATH] = {0}; 

// Prototipo para funcoes auxiliares
static int gbv_find_document_index(const Library *lib, const char *docname);
static int gbv_persist_metadata (Library *lib);
static int compare_name (const void *a, const void *b);
static int compare_date (const void *a, const void *b);
static int compare_size (const void *a, const void *b);

/**
 * Cria o arquivo container e escreve o superbloco
 * Recebe como parametro:
 * - Nome do arquivo a ser criado (filename)
 * return 0 sucesso, -1 erro
*/
int gbv_create(const char *filename) {
    // Tentativa de criar em modo escrita binaria
    FILE *fp = fopen (filename, "wb");
    if (fp == NULL) {
        perror ("gbv_create: Erro ao criar o arquivo da biblioteca.\n");
        return -1;
    }

    // Prepara superbloco, é o cabeçalho principal do container
    GBV_Superblock sb;
    sb.count = 0; // Inicia com 0 docs
    sb.dir_offset = sizeof (GBV_Superblock); // O diretorio comeca apos superbloco

    // Grava superbloco no inicio do arquivo
    size_t writter = fwrite (&sb, sizeof (GBV_Superblock), 1, fp);
    if (writter != 1) {
        perror ("gbv_create: Erro ao escrever o superbloco inicial.");
        fclose (fp);
        return -1;
    }
    fclose (fp);
    printf ("Biblioteca '%s' criada com sucesso.\n", filename);

    return 0;

}

/** 
 * Abre ou cria as biblio e carrega o metadados p/ memoria
 * Recebe como parametro:
 * - Ponteiro para a estrutura Library (lib)
 * - Nome do arquivo container a ser aberto (archive)
 * return 0 sucesso, -1 erro
 */ 
int gbv_open (Library *lib, const char *filename) {
    // Tenta abrir em modo leitura/escrita binaria
    FILE *fp = fopen (filename, "r+b");
    if (fp == NULL) {
        // Arquivo nao existe, informa na tela e tenta cria-lo
        printf ("Biblioteca '%s' nao encontrada, criando uma nova...\n", filename);
        if (gbv_create (filename) != 0) {
            return -1; 
        }

        // Tenta abrir novamente arquivo recem criado
        fp = fopen (filename, "r+b");
        if (fp == NULL) {
            perror ("gbv_open: Erro fatal ao abrir a biblioteca criada.\n");
            return -1;
        }
    }

    // Armazena o nome do arquivo na variavel global para outras operacoes
    strncpy (GBV_ARCHIVE_NAME, filename, MAX_ARCHIVE_PATH - 1);
    GBV_ARCHIVE_NAME[MAX_ARCHIVE_PATH - 1] = '\0';

    // Le o superbloco do inicio do arquivo para informacoes essenciais
    GBV_Superblock sb;
    if (fread (&sb, sizeof (GBV_Superblock), 1, fp) != 1) {
        perror ("gbv_open: Erro ao ler o superbloco da biblioteca.\n");
        fclose (fp);
        return -1;
    }

    // Transfere as info. do superbloco para a estrutura Library em memoria
    lib->count = sb.count;
    lib->docs = NULL;

    // Se tem documentos carrega diretorio para memoria
    if (lib->count > 0) {
        // Aloca a memoria necessaria para diretorio
        lib->docs = (Document *) malloc (lib->count * sizeof (Document));
        if (lib->docs == NULL) {
            perror ("gbv_open: Falha na alocacao da memoria para o diretorio.\n");
            fclose (fp);
            return -1;
        }

        // Posiciona o cursor do arquivo onde diretorio esta armazenado
        if (fseek (fp, sb.dir_offset, SEEK_SET) != 0) {
            perror ("gbv_open: Erro ao posicionar para a area de diretorio.\n");
            free (lib->docs);
            fclose (fp);
            return -1;
        }

        // Le todos metadados do diretorio para estrutura em memoria
        size_t read_count = fread (lib->docs, sizeof (Document), lib->count, fp);
        if (read_count != lib->count) {
            perror ("gbv_open: Erro ao ler diretorio.\n");
            free (lib->docs);
            fclose (fp);
            return -1;
        }
    }
    fclose (fp);

    return 0;
}

/**
 * Adiciona ou substitui um documento no arquivo container
 * Recebe como parametro:
 * - Ponteiro para a estrutura da biblioteca em memoria (lib)
 * - Nome do arquivo container (archive)
 * - Nome do documento a ser adicionado (docname)
 * return 0 sucesso, -1 erro
 */
int gbv_add (Library *lib, const char *archive, const char *docname) {
    FILE *doc_fp = fopen (docname, "rb");
    if (doc_fp == NULL) {
        perror ("gbv_add: Erro ao abrir o documento de origem.\n");
        return -1;
    }

    // Determina tam do doc
    if (fseek (doc_fp, 0, SEEK_END) != 0) {
        perror ("gbv_add: Erro ao buscar tamanho do documento");
        fclose (doc_fp);
        return -1;
    }

    long doc_size = ftell (doc_fp);
    if (doc_size < 0) {
        perror ("gbv_add: ftell falhou para o documento");
        fclose (doc_fp);
        return -1;
    }
    rewind (doc_fp);

    FILE *archive_fp = fopen (archive, "r+b");
    if (archive_fp == NULL) {
        perror ("gbv_add: Erro ao abrir o arquivo de biblioteca");
        fclose (doc_fp);
        return -1;
    }

    GBV_Superblock sb;
    if (fread (&sb, sizeof (GBV_Superblock), 1, archive_fp) != 1) {
        perror ("gbv_add: Erro ao ler o superbloco");
        fclose (doc_fp);
        fclose (archive_fp);
        return -1;
    }

    // Anexa no final
    if (fseek (archive_fp, 0, SEEK_SET) != 0) {
        perror ("gbv_add: Erro ao posionar para o final de dados");
        fclose (doc_fp);
        fclose (archive_fp);
        return -1;
    }

    fseek (archive_fp, 0, SEEK_END);
    long new_doc_offset = ftell (archive_fp);
    printf("printf do ftell %ld\n", new_doc_offset);
    if (new_doc_offset < 0) {
        perror ("gbv_add: ftell falhou no archive");
        fclose (doc_fp);
        fclose (archive_fp);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread (buffer, 1, BUFFER_SIZE, doc_fp)) > 0) {
        size_t bytes_written = fwrite (buffer, 1, bytes_read, archive_fp);
        if (bytes_written != bytes_read) {
            perror ("gbv_add: Erro ao escrever dados no container");
            fclose (doc_fp);
            fclose (archive_fp);
            return -1;
        }
    }
    fclose (doc_fp);

    long new_dir_offset = ftell (archive_fp);
    if (new_dir_offset < 0) {
        perror ("gbv_add: ftell falhou ao obter new_dir_offset");
        fclose (archive_fp);
        return -1;
    }

    // Atualiza ou insere entrada no diretorio em memoria
    int index = gbv_find_document_index (lib, docname);
    time_t now = time (NULL);
    if (index != -1) {
        lib->docs[index].size = doc_size;
        lib->docs[index].date = now;
        lib->docs[index].offset = new_doc_offset;
    } else {
        int new_count = lib->count + 1;
        Document *new_docs = (Document *) realloc (lib->docs, new_count * sizeof (Document));
        if (new_docs == NULL) {
            perror ("gbv_add: Erro ao realocar memoria");
            fclose (archive_fp);
            return -1;
        }
        lib->docs = new_docs;

        int new_id = new_count - 1;
        strncpy (lib->docs[new_id].name, docname, MAX_NAME - 1);
        lib->docs[new_id].name[MAX_NAME - 1] = '\0';
        lib->docs[new_id].size = doc_size;
        lib->docs[new_id].date = now;
        lib->docs[new_id].offset = new_doc_offset;
        lib->count = new_count;
    }

    if (fwrite (lib->docs, sizeof (Document), lib->count, archive_fp) != (size_t) lib->count) {
        perror ("gbv_add: Erro ao escrever a atualizacao do diretorio no conteiner");
        fclose (archive_fp);
        return -1;
    }

    sb.count = lib->count;
    sb.dir_offset = new_dir_offset;

    rewind (archive_fp);
    if (fwrite (&sb, sizeof (GBV_Superblock), 1, archive_fp) != 1) {
        perror ("gbv_add: Erro ao atualizar superbloco");
        fclose (archive_fp);
        return -1;
    }

    fclose (archive_fp);
    printf ("Documento '%s (%ld bytes) adicionado com sucesso  (offset %ld).\n", docname, doc_size, new_doc_offset);

    return 0;
}

/**
 * Remove um documento da biblioteca
 * Recebe como parametro:
 * - Ponteiro para a estrutura da biblioteca em memoria (lib)
 * - Nome do documento a ser removido (docname)
 * return 0 sucesso, -1 erro
 */
int gbv_remove (Library *lib, const char *docname) {
    // Procura pelo indice do documento a ser removido
    int index = gbv_find_document_index (lib, docname);
    if (index == -1) {
        printf ("Erro: Documento '%s' nao encontrado na biblioteca.\n", docname);
        return -1;
    }

    // Deslocando entrada do diretorio para "apagar" o membro
    // 
    for (int i = index; i < lib->count - 1; i++) {
        lib->docs[i] = lib->docs[i + 1];
    }
    
    lib->count--;

    // Realocando memoria do diretorio para tamanho menor
    if (lib->count > 0) {
        Document *new_docs = (Document *) realloc (lib->docs, lib->count * sizeof (Document));
        if (new_docs == NULL) {
            perror ("gbv_remove: Erro ao realocar diretorio apos remocao");
            return -1; 
        }
        lib->docs = new_docs;
    } else { // Se nao ha mais documentos, libera toda memoria
        free (lib->docs);
        lib->docs = NULL;
    }

    // Persiste as mudancas no arquivo, reescrevendo o diretorio e superbloco
    if (gbv_persist_metadata (lib) != 0) {
        printf ("Erro ao salvar as alteracoes no arquivo apos remocao.\n");
        return -1;
    }

    printf ("Documento '%s' removido com sucesso.\n", docname);
    return 0;
}

/**
 * Lista todos documetos e seus metadados contidos na biblioteca
 * Recebe como parametro:
 * - Ponteiro para estrutur da biblioteca ja carragada em memoria (lib)
 * return 0
 */
int gbv_list (const Library *lib) {
    // Verifica se a biblio está vazia
    if (lib->count == 0) {
        printf ("A biblioteca esta vazia.\n");
        return 0;
    }

    // Imprime cabeçalho da tabela
    printf ("\n--- Listando %d documento(s) na biblioteca ---\n", lib->count);
    printf ("%-30s | %-12s | %-20s | %-10s", "NOME", "TAMANHO (B)", "DATA DE INSECAO", "OFFSET");
    printf("\n----------------------------------------------------------------------------------\n");

    char date_buffer[100];

    // Itera sobre todos documentos no diretorio e imprime suas infos.
    for (int i = 0; i < lib->count; i++) {
        // Formata a data
        format_date (lib->docs[i].date, date_buffer, sizeof (date_buffer));

        printf ("%-30s | %-12ld | %-20s | %-10ld\n",
                lib->docs[i].name,
                lib->docs[i].size,
                date_buffer,
                lib->docs[i].offset);
    }
    printf("\n----------------------------------------------------------------------------------\n");

    return 0;
}

/**
 * Vizualiza o conteudo de um documento em blocos com tam. fixo
 * Recebe como parametro:
 * - Ponteiro para a estrutura da biblioteca (lib)
 * - Nome do documento a ser visualizado (docname)
 * return 0 sucesso, -1 erro
 */
int gbv_view (const Library *lib, const char *docname) {
    int index = gbv_find_document_index (lib, docname);
    if (index == -1) {
        printf ("Erro: Documento '%s' nao encontrado na biblioteca.\n", docname);
        return -1;
    }

    FILE *fp = fopen (GBV_ARCHIVE_NAME, "rb");
    if (fp == NULL ) {
        perror ("gbv_view: Erro ao abrir a biblioteca para visualizacao.\n");
        return -1;
    }

    // Obetem infos do documento do diretorio
    long doc_offset = lib->docs[index].offset;
    long doc_size = lib->docs[index].size;
    long current_pos = 0; // Posicao atual de visualizacao dentro do doc

    char buffer[BUFFER_SIZE];
    char command;

    // Loop de navegacao
    do {
        // Exibe cabeçalho com infos e comandos
        printf ("\n--- Visualizando '%s' (Tamanho: %ld bytes) | Exibindo a partir do byte: %ld ---\n", 
                docname, doc_size, current_pos);
        printf("--- Comandos: [n] próximo bloco, [p] bloco anterior, [q] sair ---\n\n");
        
        // Posiciona o cursor no local exato do bloco a ser lido
        if (fseek (fp, doc_offset + current_pos, SEEK_SET) != 0) {
            perror ("gbv_view: Erro ao posicionar o ponteiro de leitura.\n");
            break;
        }

        // Calcula quantos bytes ler para nao ultrapassar final do doc
        size_t to_read = (size_t) ((doc_size - current_pos) < BUFFER_SIZE ? (doc_size - current_pos) : BUFFER_SIZE);
        size_t bytes_read = fread (buffer, 1, to_read, fp);
        if (bytes_read > 0) { 
            // Imprime conteudo do buffer diretaente na saida padrao
            fwrite (buffer, 1, bytes_read, stdout);
        }
        
        // Tentar substituir 'scanf' por 'fgets' para evitar erros no buffer
        printf ("\n\nComando> ");
        char input[8];
        if (fgets (input, sizeof (input), stdin) == NULL) {
            command = 'q';
        } else {
            command = input[0];
        }

        switch (command) {
            case 'n': // Próximo
                if (current_pos + BUFFER_SIZE < doc_size) {
                    current_pos += BUFFER_SIZE;
                } else {
                    printf("Já está no último bloco.\n");
                }
                break;
            case 'p': // Anterior
                if (current_pos - BUFFER_SIZE >= 0) {
                    current_pos -= BUFFER_SIZE;
                } else {
                    printf("Já está no primeiro bloco.\n");
                }
                break;
            case 'q': // Sair
                printf("Saindo da visualização...\n");
                break;
            default:
                printf("Comando inválido.\n");
                break;
        }
    } while (command != 'q');

    fclose (fp);
    return 0;
}

/**
 * Reordena os documentos na biblioteca com base em um criterio
 * Recebe como parametro:
 * - Ponteiro para estrutura da biblioteca (lib) 
 * - Nome do arquivo contaier (archive (pode ser usado para reescrever dados))
 * - String que define o criterio: "nome", "data" ou "tamanho"
 * retur 0 sucesso, -1 erro
 */
int gbv_order (Library *lib, const char *archive, const char *criteria) {
    if (lib->count < 2) {
        printf ("Nao ha documentos suficientes para ordenar.\n");
        return 0;
    }
    printf ("Reordenando a biblioteca por '%s' ...\n", criteria);

    // Usa qsort da biblioteca padrao para ordenar o diretorio em memoria
    if (strcmp (criteria, "nome") == 0) {
         qsort(lib->docs, lib->count, sizeof(Document), compare_name);
    } else if (strcmp(criteria, "data") == 0) {
        qsort(lib->docs, lib->count, sizeof(Document), compare_date);
    } else if (strcmp(criteria, "tamanho") == 0) {
        qsort(lib->docs, lib->count, sizeof(Document), compare_size);
    } else {
        printf("Erro: Critério de ordenação invalido: '%s'.\n", criteria);
        printf("Use 'nome', 'data' ou 'tamanho'.\n");
        return -1;
    }

    // Funcao reordena apenas os metadados no diretorio
    // Dados fisicos no arquivo container nao sao movidos
    // Ordem de acesso aos arquivos mudará, layout permanece o mesmo

    // Persiste o diretorio reodenado no disco
    if (gbv_persist_metadata (lib) != 0) {
        printf ("Erro ao salvar a biblioteca reordenada no disco.\n");
        return -1;
    }

    printf("Biblioteca reordenada com sucesso.\n");
    return 0;
}

/**
 * Libera memoria alocada para o diretorio da biblioteca
 * Recebe como parametro:
 * - Ponteiro para a estrutura da biblioteca (lib)
 */
void gbv_close (Library *lib) {
    if (lib->docs != NULL) {
        free (lib->docs);
        lib->docs = NULL;
        // Liberar lib->archive_name
    }
    lib->count = 0;
}

//----------------------------------------------------------------------------------------//
// FUNCOES AUXILIARES
//----------------------------------------------------------------------------------------//

/** Encontra indice de um documento no diretorio pelo nome
 * Recebe como parametro:
 * - Ponteiro para biblioteca (lib)
 * - Nome do documento a ser encontrado (docname)
 * return indice do documento se encontrado, -1 caso contrario
 */
static int gbv_find_document_index(const Library *lib, const char *docname) {
    for (int i = 0; i < lib->count; i++) {
        if (strcmp (lib->docs[i].name, docname) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Grava os metadados da memoria para disco
 * Recebe como parametro:
 * - Ponteiro para a aestrutura da biblioteca com dados atualizados
 * return 0 sucesso, -1 erro
 */
static int gbv_persist_metadata (Library *lib) {
    FILE *fp = fopen (GBV_ARCHIVE_NAME, "r+b");
    if (fp == NULL) {
        perror ("gbv_persist_metadata: Erro ao abrir biblioteca para salvar.\n");
        return -1;
    }

    GBV_Superblock sb;
    if (fread (&sb, sizeof (GBV_Superblock), 1, fp) != 1) {
        perror ("gbv_persist_metadata: Erro ao ler superbloco.\n");
        fclose (fp);
        return -1;
    }

    if (fseek (fp, sb.dir_offset, SEEK_SET) != 0) {
        perror ("gbv_persist_metadata: Erro ao buscar a posicao do diretorio.\n");
        fclose (fp);
        return -1;
    }

    if (lib->count > 0) {
        if (fwrite (lib->docs, sizeof (Document), lib->count, fp) != (size_t) lib->count) {
            perror ("gbv_persist_metadata: Erro ao escrever novo diretorio.\n");
            fclose (fp);
            return -1;
        }
    }

    sb.count = lib->count;

    rewind (fp);
    if (fwrite (&sb, sizeof (GBV_Superblock), 1, fp) != 1) {
        perror ("gbv_persist_metadata: Erro ao atualizar o superbloco.\n");
        fclose (fp);
        return -1;
    }

    fclose (fp);
    return 0;
}

//---------------------------------------------------------------------------------------------------------//
// FUNÇÕES DE COMPARAÇÃO PARA qsort
//---------------------------------------------------------------------------------------------------------//

// Compara por nome
static int compare_name (const void *a, const void *b) {
    Document *doc_a = (Document *) a;
    Document *doc_b = (Document *) b;

    return strcmp (doc_a->name, doc_b->name);
}

// Compara por data
static int compare_date (const void *a, const void *b) {
    Document *doc_a = (Document *) a;
    Document *doc_b = (Document *) b;

    if (doc_a->date < doc_b->date)
        return -1;
    if (doc_a->date > doc_b->date)
        return 1;
    
    return 0;
}

// Compara por tamanho
static int compare_size (const void *a, const void *b) {
    Document *doc_a = (Document *) a;
    Document *doc_b = (Document *) b;

    if (doc_a->size < doc_b->size)
        return -1;
    if (doc_a->size > doc_b->size)
        return 1;
    
    return 0;
}
