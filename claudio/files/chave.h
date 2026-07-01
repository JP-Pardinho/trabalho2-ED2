/*
 * chave.h
 * Define a chave de busca composta usada pelo sistema de RH:
 * Nome (critério principal) + Data de Nascimento (critério de desempate).
 *
 * Este arquivo NÃO faz parte da Árvore B+ genérica (Bplus.h/Bplus.c).
 * Ele apenas implementa as funções de callback exigidas pela árvore.
 */

#ifndef CHAVE_H
#define CHAVE_H

#include "Bplus.h"

#define TAM_NOME 100

typedef struct {
    int dia;
    int mes;
    int ano;
} Data;

typedef struct {
    char nome[TAM_NOME];
    Data nascimento;
} ChaveFuncionario;

/* Cria uma chave a partir de nome e data (facilita o uso no main.c) */
ChaveFuncionario* criar_chave(const char *nome, int dia, int mes, int ano);

/* Callbacks exigidos pela Árvore B+ genérica */
int   chave_comparar(const void *chaveA, const void *chaveB);
void  chave_serializar(const void *chave, unsigned char *buffer);
void* chave_deserializar(const unsigned char *buffer);
int   chave_tamanho(void);
void  chave_liberar(void *chave);
void  chave_imprimir(const void *chave);

#endif
