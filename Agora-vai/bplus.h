/* ============================================================================
 * bplus.h
 *
 * Arvore B+ GENERICA em disco.
 *
 * Esta implementacao nao conhece nenhum tipo de dado especifico (ex: Funcionario).
 * Toda a especializacao eh feita atraves de PONTEIROS DE FUNCAO (callbacks) que
 * o modulo cliente (main.c / funcionario.c) fornece no momento da abertura/criacao
 * da arvore.
 *
 * Callbacks exigidos:
 *   - bplus_cmp_fn   : compara duas chaves (retorna <0, 0, >0)
 *   - bplus_size_fn  : retorna o tamanho em bytes de um registro (chave+dados)
 *                      serializado, para que a arvore saiba quanto espaco alocar
 *                      no arquivo de dados.
 *   - bplus_write_fn : serializa um registro (void*) para um buffer de bytes
 *   - bplus_read_fn  : deserializa um buffer de bytes para um registro (void*)
 *   - bplus_key_fn   : extrai a chave de um registro completo (void*)
 *   - bplus_print_fn : imprime um resumo textual de um registro (usado na
 *                      impressao hierarquica da arvore)
 *
 * A arvore armazena, em cada folha, um PONTEIRO DE DISCO (offset em bytes,
 * tipo long) para o registro completo, que fica gravado em um ARQUIVO DE DADOS
 * separado (gerenciado pelo cliente, ex: funcionarios.dat). A arvore em si
 * (arquivo de indice) so guarda CHAVES + OFFSETS, mantendo-se genérica.
 * ==========================================================================*/

#ifndef BPLUS_H
#define BPLUS_H

#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * Configuracao de ordem da arvore.
 * ORDEM = numero maximo de filhos de um no interno.
 * Numero maximo de chaves por no = ORDEM - 1.
 * ------------------------------------------------------------------------- */
#define BPLUS_ORDEM 5
#define BPLUS_MAX_CHAVES (BPLUS_ORDEM - 1)
#define BPLUS_MIN_CHAVES ((BPLUS_ORDEM % 2 == 0) ? (BPLUS_ORDEM/2 - 1) : (BPLUS_ORDEM/2))

/* Tamanho maximo (em bytes) que uma chave serializada pode ocupar dentro do
 * no da arvore B+. O cliente deve garantir que sua chave serializada nao
 * exceda esse valor. */
#define BPLUS_TAM_MAX_CHAVE 128

/* Ponteiro de disco nulo */
#define BPLUS_NULL -1L

/* ---------------------------------------------------------------------------
 * Tipos de callback genericos
 * ------------------------------------------------------------------------- */

/* Compara duas chaves ja deserializadas (void*). Retorna <0, 0 ou >0. */
typedef int (*bplus_cmp_fn)(const void *chaveA, const void *chaveB);

/* Serializa uma chave (void*) para o buffer 'buf'. Deve escrever exatamente
 * bplus_key_size_fn() bytes. */
typedef void (*bplus_key_write_fn)(const void *chave, unsigned char *buf);

/* Deserializa uma chave a partir do buffer 'buf' para uma area alocada
 * dinamicamente (retornada). O chamador (a arvore) deve depois liberar
 * com free() quando nao precisar mais, OU o cliente pode gerenciar cache -
 * nesta implementacao a arvore libera sempre apos o uso local. */
typedef void *(*bplus_key_read_fn)(const unsigned char *buf);

/* Tamanho fixo em bytes da chave serializada. */
typedef int (*bplus_key_size_fn)(void);

/* Libera a memoria de uma chave alocada por bplus_key_read_fn */
typedef void (*bplus_key_free_fn)(void *chave);

/* Imprime uma representacao textual curta da chave (para debug / listagem
 * da estrutura da arvore). */
typedef void (*bplus_key_print_fn)(const void *chave);

/* ---------------------------------------------------------------------------
 * Estrutura de configuracao da arvore (conjunto de callbacks + arquivo)
 * ------------------------------------------------------------------------- */
typedef struct {
    FILE *arquivo;              /* arquivo de indice (nos da arvore) em disco */
    char caminho[256];          /* caminho do arquivo de indice */
    long raiz;                  /* offset do no raiz no arquivo, ou BPLUS_NULL */
    long topo;                  /* offset do "fim" do arquivo (proximo bloco livre p/ crescimento linear) */
    long lista_livres;          /* cabeca da lista encadeada de blocos livres reaproveitaveis */

    bplus_cmp_fn cmp;
    bplus_key_write_fn key_write;
    bplus_key_read_fn key_read;
    bplus_key_size_fn key_size;
    bplus_key_free_fn key_free;
    bplus_key_print_fn key_print;

    int tam_chave;   /* cache de key_size() */
    int tam_no;      /* tamanho fixo (bytes) de um no serializado em disco */
} BPlusTree;

/* ---------------------------------------------------------------------------
 * API publica
 * ------------------------------------------------------------------------- */

/* Cria (ou recria do zero) uma nova arvore B+ em disco no caminho informado.
 * Sobrescreve arquivo existente. */
BPlusTree *bplus_criar(const char *caminho,
                        bplus_cmp_fn cmp,
                        bplus_key_write_fn key_write,
                        bplus_key_read_fn key_read,
                        bplus_key_size_fn key_size,
                        bplus_key_free_fn key_free,
                        bplus_key_print_fn key_print);

/* Abre uma arvore B+ existente em disco (ou cria se nao existir). */
BPlusTree *bplus_abrir(const char *caminho,
                        bplus_cmp_fn cmp,
                        bplus_key_write_fn key_write,
                        bplus_key_read_fn key_read,
                        bplus_key_size_fn key_size,
                        bplus_key_free_fn key_free,
                        bplus_key_print_fn key_print);

/* Fecha a arvore, gravando o cabecalho (raiz, topo, lista de livres) em disco. */
void bplus_fechar(BPlusTree *arvore);

/* Insere um par (chave, offset_do_registro_no_arquivo_de_dados).
 * Retorna 1 em caso de sucesso, 0 se a chave ja existir (nao insere duplicata). */
int bplus_inserir(BPlusTree *arvore, const void *chave, long offset_dado);

/* Busca uma chave exata. Se encontrada, escreve o offset do registro em
 * *offset_saida e retorna 1. Caso contrario retorna 0. */
int bplus_buscar(BPlusTree *arvore, const void *chave, long *offset_saida);

/* Remove uma chave exata da arvore. Retorna 1 se removido, 0 se nao encontrado. */
int bplus_remover(BPlusTree *arvore, const void *chave);

/* Atualiza o offset de dado associado a uma chave existente (usado quando o
 * registro e reescrito em outra posicao do arquivo de dados, por exemplo). */
int bplus_atualizar_offset(BPlusTree *arvore, const void *chave, long novo_offset);

/* Busca por intervalo aberto (chaveA, chaveB): chama callback 'visitar' para
 * cada par (chave, offset) cuja chave satisfaz chaveA < chave < chaveB.
 * O callback recebe a chave (void*, NAO deve ser liberada pelo callback) e o
 * offset do dado, alem de um ponteiro de contexto livre (ctx). */
typedef void (*bplus_visit_fn)(const void *chave, long offset_dado, void *ctx);
void bplus_buscar_intervalo(BPlusTree *arvore, const void *chaveA, const void *chaveB,
                             bplus_visit_fn visitar, void *ctx);

/* Percorre TODA a arvore em ordem (util para varreduras completas). */
void bplus_percorrer(BPlusTree *arvore, bplus_visit_fn visitar, void *ctx);

/* Imprime hierarquicamente a estrutura da arvore (nivel por nivel, indentado),
 * usando o callback key_print para representar cada chave. */
void bplus_imprimir_estrutura(BPlusTree *arvore);

#endif /* BPLUS_H */
