#ifndef ARVOREB_H
#define ARVOREB_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Definição da ordem da árvore
#define P 3          // ordem interno (p)
#define PFOLHA 2     // ordem folha (pfolha)

// =======================================================
// 1. Definição do Callback de Comparação
// =======================================================
/*
 * O callback de comparação diz à árvore como comparar duas chaves desconhecidas.
 * Deve retornar:
 * < 0 se a < b
 * 0 se a == b
 * > 0 se a > b
 */
typedef int (*ComparaChaveFunc)(const void *a, const void *b);

// =======================================================
// 2. Estrutura de Controle da Árvore
// =======================================================
/*
 * Guarda os metadados necessários para operar no arquivo em disco.
 * Em vez de passar a "raiz" para toda função, passaremos este controlador.
 */
typedef struct {
    FILE *arquivo;            // Ponteiro para o arquivo binário aberto
    long offset_raiz;         // Posição em bytes de onde a raiz está no arquivo
    size_t tamanho_chave;     // Tamanho em bytes do tipo da chave (ex: sizeof(ChaveRH))
    size_t tamanho_registro;  // Tamanho em bytes do tipo do dado (ex: sizeof(Funcionario))
    ComparaChaveFunc compara; // Função injetada para comparar as chaves
} ArvoreBPlus;

// =======================================================
// 3. Estrutura do Nó (O que vai para o disco)
// =======================================================
/*
 * Como o tamanho da chave e dos dados é variável, o nó em si não pode ser 
 * uma struct estática simples se quisermos um código limpo.
 * Essa struct abaixo serve como CABEÇALHO do bloco que vai pro disco.
 */
typedef struct {
    int folha;                // 1 se for folha, 0 se interno
    int n;                    // Quantidade atual de chaves
    long offset_pai;          // "Ponteiro" para o pai em disco (-1 se for raiz)
    long offset_prox_folha;   // "Ponteiro" para a próxima folha (útil para busca por intervalo)
} CabecalhoNo;

// =======================================================
// 4. Protótipos da API Pública
// =======================================================

// Inicializa a árvore, abre o arquivo e grava metadados iniciais se necessário
ArvoreBPlus* criar_arvore(const char *nome_arquivo, size_t tam_chave, size_t tam_registro, ComparaChaveFunc func_compara);

// Fecha o arquivo e libera a estrutura de controle da RAM
void fechar_arvore(ArvoreBPlus *arvore);

// Funções principais que agora recebem ponteiros genéricos
bool inserir_registro(ArvoreBPlus *arvore, void *chave, void *registro);
bool buscar_registro(ArvoreBPlus *arvore, void *chave, void *registro_retorno);
bool remover_registro(ArvoreBPlus *arvore, void *chave);

// =======================================================
// 5. Protótipos de Operação em Disco (Uso interno)
// =======================================================

// Lê um nó do disco a partir de um offset e carrega para a RAM temporariamente
void ler_no(ArvoreBPlus *arvore, long offset, CabecalhoNo *cabecalho, void *chaves, void *dados, long *filhos);

// Grava um nó da RAM para o disco em um offset específico
void gravar_no(ArvoreBPlus *arvore, long offset, CabecalhoNo *cabecalho, void *chaves, void *dados, long *filhos);

// =======================================================
// Callback para processar registros encontrados
// =======================================================
/*
 * Essa função será definida por você no main.c ou funcionario.c.
 * A árvore vai chamá-la para cada registro que estiver dentro do intervalo,
 * passando os ponteiros genéricos da chave e do dado.
 */
typedef void (*ProcessaRegistroFunc)(void *chave, void *registro);

// =======================================================
// Protótipo da Busca por Intervalo
// =======================================================
void buscar_intervalo(ArvoreBPlus *arvore, void *chave_inicio, void *chave_fim, ProcessaRegistroFunc processar);

// =======================================================
// Callback para imprimir uma chave na tela
// =======================================================
typedef void (*ImprimeChaveFunc)(void *chave);

// =======================================================
// Protótipos Finais do Motor
// =======================================================
void imprimir_arvore(ArvoreBPlus *arvore, ImprimeChaveFunc imprime_chave);
bool remover_registro(ArvoreBPlus *arvore, void *chave);

#endif