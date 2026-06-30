#ifndef BPLUS_H
#define BPLUS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum { 
    INTERNA = 0, 
    FOLHA = 1
} TipoPagina;

// =======================================================
// Callbacks Exigidos pelo PDF (Genericidade)
// =======================================================
// Callback para comparar duas chaves genéricas
typedef int (*ComparaChaveFunc)(const void *a, const void *b);

// Callback para imprimir uma chave genérica no terminal
typedef void (*ImprimeChaveFunc)(const void *chave);

// Callback para processar o resultado de uma busca por intervalo
typedef void (*ProcessaRegistroFunc)(const void *chave, int index_registro);

// =======================================================
// Estruturas de Dados do Disco
// =======================================================
typedef struct {
    int ordem;
    int qtdPaginas;
    int raiz;
    int qtdRegistros;
    size_t tamanho_chave; 
    size_t tamanho_no;    
} CabecalhoArvore;

typedef struct {
    FILE *arquivo_indice;
    CabecalhoArvore cabecalho;
    ComparaChaveFunc compara;
} ArvoreBPlus;

typedef struct {
    TipoPagina tipo;
    int qtdElementos;
    int pai;
    int indexProximaPagina;
    int indexPaginaAnterior;
    int nivel;
    int index;
    int foiDeletada;
} CabecalhoPagina;

// Estrutura de paginação temporária na RAM 
typedef struct {
    CabecalhoPagina cabecalho;
    void *chaves;  
    int *filhos;   
} PaginaRAM;

// =======================================================
// API Pública da Árvore B+ Genérica
// =======================================================
ArvoreBPlus* criar_arvore(const char *nome_arquivo, int ordem, size_t tamanho_chave, ComparaChaveFunc func_compara);
void fechar_arvore(ArvoreBPlus *arvore);

// Operações Principais
bool inserir_registro(ArvoreBPlus *arvore, void *chave, int index_registro);
bool buscar_registro(ArvoreBPlus *arvore, void *chave, int *index_retorno);
bool remover_registro(ArvoreBPlus *arvore, void *chave);

// Funcionalidades Específicas do Menu (Busca por Intervalo e Impressão)
void buscar_intervalo(ArvoreBPlus *arvore, void *chave_inicio, void *chave_fim, ProcessaRegistroFunc processar);
void imprimir_arvore(ArvoreBPlus *arvore, ImprimeChaveFunc imprime_chave);

#endif // BPLUS_H