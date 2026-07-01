/*
 * funcionario.h
 * Struct do funcionário e funções para gravar/ler os registros no
 * arquivo de dados em disco (dados.bin). A Árvore B+ guarda apenas
 * o endereço (offset) de cada registro neste arquivo.
 */

#ifndef FUNCIONARIO_H
#define FUNCIONARIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "chave.h"

#define TAM_ENDERECO 150
#define TAM_TELEFONE 20
#define MAX_HISTORICO 12

typedef struct {
    Data data_pagamento;
    float valor;
} Pagamento;

typedef struct {
    char nome[TAM_NOME];
    Data nascimento;
    char nome_mae[TAM_NOME];
    char nome_pai[TAM_NOME];
    char endereco[TAM_ENDERECO];
    char telefone[TAM_TELEFONE];

    Data data_contratacao;
    bool ativo;
    Data data_desligamento;

    Pagamento historico[MAX_HISTORICO];
    int qtd_pagamentos;
} Funcionario;

/* Abre (ou cria) o arquivo binário onde ficam os dados dos funcionários */
FILE* abrir_arquivo_dados(const char *caminho);

/* Grava um novo funcionário no final do arquivo e devolve o endereço (offset) gravado */
long gravar_funcionario(FILE *arq, const Funcionario *f);

/* Sobrescreve um funcionário já existente no mesmo endereço (usado no update) */
void atualizar_funcionario(FILE *arq, long endereco, const Funcionario *f);

/* Lê um funcionário do arquivo a partir do endereço informado */
Funcionario ler_funcionario(FILE *arq, long endereco);

/* Imprime a ficha completa do funcionário (com histórico de pagamentos) */
void imprimir_ficha_completa(const Funcionario *f);

/* Imprime os dados cadastrais, sem o histórico de pagamentos (usado na exclusão) */
void imprimir_ficha_resumida(const Funcionario *f);

#endif
