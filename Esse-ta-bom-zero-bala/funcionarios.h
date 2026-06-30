#ifndef FUNCIONARIO_H
#define FUNCIONARIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =======================================================
// Estruturas de Dados Base
// =======================================================

// Estrutura para datas exigida no requisito
typedef struct {
    int dia;
    int mes;
    int ano;
} Data;

// A Chave Composta que será enviada para a Árvore B+
// Requisito: Nome (critério principal) e Data de Nascimento (desempate)
typedef struct {
    char nome[100];
    Data nascimento;
} ChaveFuncionario;

// A Ficha completa do Funcionário que será guardada no ficheiro de dados
typedef struct {
    char nome[100];
    Data nascimento;
    
    char nomeMae[100];
    char nomePai[100];
    
    char endereco[200];
    char telefone[20];
    
    Data contratacao;
    int ativo; // 1 para Ativo, 0 para Inativo
    Data desligamento; // Apenas se ativo == 0
    
    // Vetor estático para os últimos 12 meses trabalhados
    float historicoPagamentos[12]; 
} Funcionario;

// =======================================================
// Callbacks para a Árvore B+
// =======================================================

/*
 * Compara duas Chaves Compostas. 
 * Retorna < 0 se a < b, 0 se a == b, e > 0 se a > b.
 */
int comparar_chaves_funcionario(const void *a, const void *b);

/*
 * Imprime uma Chave Composta no ecrã (usada na exibição hierárquica da árvore)
 */
void imprimir_chave_funcionario(void *chave);

#endif // FUNCIONARIO_H