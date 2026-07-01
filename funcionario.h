#ifndef FUNCIONARIO_H
#define FUNCIONARIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Bplus.h"

typedef struct {
    int dia;
    int mes;
    int ano;
} Data;

typedef struct {
    char nome[100];
    Data nascimento;
} ChaveFuncionario;

typedef struct {
    Data dataPagamento;
    float valor;
} Pagamento;

typedef struct {
    char nome[100];
    Data nascimento;
    char nomeMae[100];
    char nomePai[100];
    char endereco[150];
    char telefone[20];
    Data dataContratacao;
    bool ativo;
    Data dataDesligamento;
    Pagamento historico[12];
    int qtdPagamentos;
} Funcionario;

FILE *abrirArquivoDados(const char *caminho);
long gravarFuncionario(FILE *arq, const Funcionario *f);
void atualizarFuncionario(FILE *arq, long endereco, const Funcionario *f);
Funcionario lerFuncionario(FILE *arq, long endereco);
void imprimirFichaCompleta(const Funcionario *f);
void imprimirFichaResumida(const Funcionario *f);

#endif