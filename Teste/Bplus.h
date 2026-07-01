#ifndef BPLUS_H
#define BPLUS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Configurações da Árvore
#define ORDEM 5
#define MAX_CHAVES (ORDEM - 1)
#define MIN_CHAVES (MAX_CHAVES / 2)
#define TAM_MAX_CHAVE 128
#define ENDERECO_NULO -1L

/* ============================================================================
 * CALLBACKS (Ponteiros de Função)
 * Exigência do PDF para manter a árvore estritamente genérica.
 * ========================================================================= */
typedef int  (*CompararChavesFn)(const void *chaveA, const void *chaveB);
typedef void (*SerializarChaveFn)(const void *chave, unsigned char *buffer);
typedef void*(*DeserializarChaveFn)(const unsigned char *buffer);
typedef int  (*TamanhoChaveFn)(void);
typedef void (*LiberarChaveFn)(void *chave);
typedef void (*ImprimirChaveFn)(const void *chave);

/* ============================================================================
 * ESTRUTURA PRINCIPAL DA ÁRVORE
 * ========================================================================= */
typedef struct {
    FILE *arquivo_disco;
    long endereco_raiz;
    long proximo_bloco_livre; // Para gerenciar o fim do arquivo ou blocos deletados
    
    // Callbacks do cliente (RH)
    CompararChavesFn comparar;
    SerializarChaveFn serializar;
    DeserializarChaveFn deserializar;
    TamanhoChaveFn tamanho;
    LiberarChaveFn liberar;
    ImprimirChaveFn imprimir;
    
    int tamanho_no_bytes; // Calculado dinamicamente para saber o quanto ler/gravar
} ArvoreBPlus;

/* ============================================================================
 * API DA ÁRVORE (Funções Públicas)
 * ========================================================================= */

// Inicializa a árvore criando ou abrindo o arquivo binário
ArvoreBPlus* criar_arvore(const char *caminho_arquivo, 
                          CompararChavesFn cmp, SerializarChaveFn ser, 
                          DeserializarChaveFn des, TamanhoChaveFn tam, 
                          LiberarChaveFn lib, ImprimirChaveFn imp);

void fechar_arvore(ArvoreBPlus *arvore);

// Operações principais de disco
bool inserir_chave(ArvoreBPlus *arvore, const void *chave, long endereco_registro_rh);
bool buscar_chave(ArvoreBPlus *arvore, const void *chave, long *endereco_retorno);
bool remover_chave(ArvoreBPlus *arvore, const void *chave);

// Busca por intervalo e impressão hierárquica
typedef void (*VisitarNoFn)(const void *chave, long endereco_registro, void *contexto);
void buscar_intervalo(ArvoreBPlus *arvore, const void *chaveA, const void *chaveB, VisitarNoFn visitar, void *contexto);
void imprimir_estrutura_arvore(ArvoreBPlus *arvore);

#endif