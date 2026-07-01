#ifndef FUNCIONARIO_H
#define FUNCIONARIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Bplus.h"

#define TAM_NOME 100
#define TAM_ENDERECO 150
#define TAM_TELEFONE 20
#define MAX_HISTORICO 12

typedef struct {
    int dia;
    int mes;
    int ano;
} Data;

typedef struct {
    char nome[TAM_NOME];
    Data nascimento;
} ChaveFuncionario;

typedef struct {
    Data dataPagamento;
    float valor;
} Pagamento;

typedef struct {
    char nome[TAM_NOME];
    Data nascimento;
    char nomeMae[TAM_NOME];
    char nomePai[TAM_NOME];
    char endereco[TAM_ENDERECO];
    char telefone[TAM_TELEFONE];
    Data dataContratacao;
    bool ativo;
    Data dataDesligamento;
    Pagamento historico[MAX_HISTORICO];
    int qtdPagamentos;
} Funcionario;

FILE* abrirArquivoDados(const char *caminho);
long gravarFuncionario(FILE *arq, const Funcionario *f);
void atualizarFuncionario(FILE *arq, long endereco, const Funcionario *f);
Funcionario lerFuncionario(FILE *arq, long endereco);
void imprimirFichaCompleta(const Funcionario *f);
void imprimirFichaResumida(const Funcionario *f);

#endif