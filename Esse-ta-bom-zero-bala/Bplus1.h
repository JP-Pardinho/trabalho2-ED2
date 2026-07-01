#ifndef BPLUS_TREE_H
#define BPLUS_TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum { 
    INTERNA, 
    FOLHA
} TipoPagina;

// =======================================================
// Callbacks Genéricos
// =======================================================
// Função injetada pelo main.c para comparar chaves desconhecidas
typedef int (*ComparaChaveFunc)(const void *a, const void *b);

// Chamado pela busca por intervalo quando encontra um registro válido
typedef void (*ProcessaRegistroFunc)(void *chave, int index_registro);

// Chamado pela impressão da árvore para formatar a chave no printf
typedef void (*ImprimeChaveFunc)(void *chave);

// =======================================================
// Estruturas de Controle (Antigo BP_Tree e Pagina)
// =======================================================

// Substitui o seu antigo BP_Tree. Isso vai no offset 0 do arquivo.
typedef struct {
    int ordem;
    int qtdPaginas;
    int raiz;
    int qtdRegistros; // Antigo qtdPacientes
    
    // NOVOS: Necessários para o cálculo de disco genérico
    size_t tamanho_chave; 
    size_t tamanho_no;    // Quantos bytes uma página ocupa fisicamente
} CabecalhoArvore;

// Variável de controle em memória RAM. 
// Mantém o arquivo aberto e a função de comparação acessível.
typedef struct {
    FILE *arquivo_indice;
    CabecalhoArvore cabecalho;
    ComparaChaveFunc compara;
} ArvoreBPlus;

// Substitui a sua antiga struct Pagina.
// Como as chaves têm tamanho variável, essa struct é apenas o cabeçalho físico da página.
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

// Estrutura de paginação temporária na RAM (O Segredo para simplificar o Bplus.c)
typedef struct {
    CabecalhoPagina cabecalho;
    void *chaves;  // Array genérico dinâmico
    int *filhos;   // Array de índices (ORDEM + 2)
} PaginaRAM;

// =======================================================
// API Pública do Motor
// =======================================================
ArvoreBPlus* criar_arvore(const char *nome_arquivo, int ordem, size_t tamanho_chave, ComparaChaveFunc func_compara);
void fechar_arvore(ArvoreBPlus *arvore);

bool inserir_registro(ArvoreBPlus *arvore, void *chave, int index_registro);
bool buscar_registro(ArvoreBPlus *arvore, void *chave, int *index_retorno);
bool remover_registro(ArvoreBPlus *arvore, void *chave);

void buscar_intervalo(ArvoreBPlus *arvore, void *chave_inicio, void *chave_fim, ProcessaRegistroFunc processar);
void imprimir_arvore(ArvoreBPlus *arvore, ImprimeChaveFunc imprime_chave);

// =======================================================
// Funções Internas de Disco e Memória
// =======================================================
PaginaRAM alocar_pagina_ram(ArvoreBPlus *arvore);
void liberar_pagina_ram(PaginaRAM *pag);

void ler_pagina(ArvoreBPlus *arvore, int index, PaginaRAM *pag);
void gravar_pagina(ArvoreBPlus *arvore, int index, PaginaRAM *pag);
int buscar_pagina_livre(ArvoreBPlus *arvore);

// =======================================================
// Funções Internas de Manipulação da Árvore
// =======================================================
void inserir_e_ordenar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, void *nova_chave, int novo_filho);
void fix_overflow(ArvoreBPlus *arvore, PaginaRAM *pagina);

void remover_e_deslocar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, int index_remocao);
void fix_underflow(ArvoreBPlus *arvore, PaginaRAM *pagina);

#endif // BPLUS_TREE_H