#ifndef BPLUS_H
#define BPLUS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ORDEM 4
#define MAX_CHAVES (ORDEM - 1)
#define MIN_CHAVES (MAX_CHAVES / 2)
#define TAM_MAX_CHAVE 128
#define ENDERECO_NULO -1L

typedef int  (*CompararChavesFn)(const void *chaveA, const void *chaveB);
typedef void (*SerializarChaveFn)(const void *chave, unsigned char *buffer);
typedef void*(*DeserializarChaveFn)(const unsigned char *buffer);
typedef int  (*TamanhoChaveFn)(void);
typedef void (*LiberarChaveFn)(void *chave);
typedef void (*ImprimirChaveFn)(const void *chave);

typedef struct {
    FILE *arquivoDisco;
    long enderecoRaiz;
    long proximoBlocoLivre; 
    
    CompararChavesFn comparar;
    SerializarChaveFn serializar;
    DeserializarChaveFn deserializar;
    TamanhoChaveFn tamanho;
    LiberarChaveFn liberar;
    ImprimirChaveFn imprimir;
    
    int tamanhoNoBytes; 
} ArvoreBPlus;

ArvoreBPlus* criarArvore(const char *caminhoArquivo, 
                         CompararChavesFn cmp, SerializarChaveFn ser, 
                         DeserializarChaveFn des, TamanhoChaveFn tam, 
                         LiberarChaveFn lib, ImprimirChaveFn imp);

void fecharArvore(ArvoreBPlus *arvore);

bool inserirChave(ArvoreBPlus *arvore, const void *chave, long enderecoRegistro);
bool buscarChave(ArvoreBPlus *arvore, const void *chave, long *enderecoRetorno);
bool removerChave(ArvoreBPlus *arvore, const void *chave);

typedef void (*VisitarNoFn)(const void *chave, long enderecoRegistro, void *contexto);
void buscarIntervalo(ArvoreBPlus *arvore, const void *chaveA, const void *chaveB, VisitarNoFn visitar, void *contexto);
void imprimirEstruturaArvore(ArvoreBPlus *arvore);

#endif