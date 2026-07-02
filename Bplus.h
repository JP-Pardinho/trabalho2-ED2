/*
    ESTRUTURA DA DADOS II - AVALIAÇÃO 2
    PROFESSORA: Dra. Luciana Lee
    ALUNOS: 
        - 2023201331 | Gabriel dos Santos Lima
        - 2023201073 | João Pedro Pardinho Rodrigues
        - 2023200798 | Nicolas Leal Espindula
*/

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

typedef int  (*CompararChaves)(const void *chaveA, const void *chaveB);
typedef void (*GravarChave)(const void *chave, unsigned char *buffer);
typedef void*(*LerChave)(const unsigned char *buffer);
typedef int  (*TamanhoChave)(void);
typedef void (*LiberarChave)(void *chave);
typedef void (*ImprimirChave)(const void *chave);

typedef struct {
    FILE *arquivoDisco;
    long enderecoRaiz;
    long proximoBlocoLivre; 
    CompararChaves comparar;
    GravarChave gravar;
    LerChave ler;
    TamanhoChave tamanho;
    LiberarChave liberar;
    ImprimirChave imprimir;
    
    int tamanhoNoBytes; 
} ArvoreBPlus;

ArvoreBPlus* criarArvore(const char *caminhoArquivo, CompararChaves cmp, GravarChave gravar, LerChave ler, TamanhoChave tam, LiberarChave lib, ImprimirChave imp);

void fecharArvore(ArvoreBPlus *arvore);

bool inserirChave(ArvoreBPlus *arvore, const void *chave, long enderecoRegistro);
bool buscarChave(ArvoreBPlus *arvore, const void *chave, long *enderecoRetorno);
bool removerChave(ArvoreBPlus *arvore, const void *chave);

typedef void (*VisitarNo)(const void *chave, long enderecoRegistro, void *contexto);
void buscarIntervalo(ArvoreBPlus *arvore, const void *chaveA, const void *chaveB, VisitarNo visitar, void *contexto);
void imprimirEstruturaArvore(ArvoreBPlus *arvore);

#endif