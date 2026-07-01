/*
    ESTRUTURA DA DADOS II - AVALIAÇÃO 2
    GRUPO: 6
    ALUNOS: 
        - João Pedro Pardinho Rodrigues
        - Nicolas Leal Espindula
        - Gabriel dos Santos Lima
*/

#ifndef FUNCIONARIO_H
#define FUNCIONARIO_H

#include "bplus.h"

#define TAM_NOME 60
#define TAM_ENDERECO 100
#define TAM_TELEFONE 20
#define MAX_HISTORICO 12  /* ultimos 12 meses de pagamento */

/* ---------------------------------------------------------------------------
 * Data (Dia/Mes/Ano)
 * ------------------------------------------------------------------------- */
typedef struct {
    int dia;
    int mes;
    int ano;
} Data;

/* ---------------------------------------------------------------------------
 * Chave composta: Nome (criterio principal) + Data de Nascimento (desempate)
 * ------------------------------------------------------------------------- */
typedef struct {
    char nome[TAM_NOME];
    Data data_nascimento;
} ChaveComposta;

/* ---------------------------------------------------------------------------
 * Registro de pagamento mensal
 * ------------------------------------------------------------------------- */
typedef struct {
    Data data_pagamento;   /* primeiro dia util do mes de referencia */
    double valor;
} Pagamento;

/* ---------------------------------------------------------------------------
 * Registro completo do Funcionario (gravado no arquivo de dados)
 * ------------------------------------------------------------------------- */
typedef struct {
    char nome[TAM_NOME];
    Data data_nascimento;

    char nome_mae[TAM_NOME];
    char nome_pai[TAM_NOME];

    char endereco[TAM_ENDERECO];
    char telefone[TAM_TELEFONE];

    Data data_contratacao;
    int ativo;                  /* 1 = Ativo, 0 = Inativo */
    Data data_desligamento;     /* valida somente se ativo == 0 */

    double salario_base;        /* usado para gerar pagamentos consolidados */

    int n_pagamentos;                       /* quantos pagamentos estao no historico (<= MAX_HISTORICO) */
    Pagamento historico[MAX_HISTORICO];     /* vetor circular/estatico dos ultimos 12 meses */

    long offset_proprio;        /* offset deste registro no arquivo de dados (cache util) */
} Funcionario;

/* ---------------------------------------------------------------------------
 * Arquivo de dados de funcionarios (registros completos)
 * ------------------------------------------------------------------------- */
typedef struct {
    FILE *arquivo;
    char caminho[256];
    long topo;             /* proximo offset livre por crescimento linear */
    long lista_livres;     /* encadeamento de blocos removidos e reaproveitaveis */
} ArquivoFuncionarios;

/* Abre (ou cria) o arquivo de dados de funcionarios. */
ArquivoFuncionarios *func_arquivo_abrir(const char *caminho);
void func_arquivo_fechar(ArquivoFuncionarios *af);

/* Grava um NOVO registro no arquivo (alocando espaco) e retorna o offset
 * onde foi gravado. */
long func_arquivo_inserir(ArquivoFuncionarios *af, const Funcionario *f);

/* Regrava um registro EXISTENTE na mesma posicao (update in place, pois o
 * registro tem tamanho fixo). */
void func_arquivo_atualizar(ArquivoFuncionarios *af, long offset, const Funcionario *f);

/* Le um registro do arquivo de dados a partir do offset informado. */
void func_arquivo_ler(ArquivoFuncionarios *af, long offset, Funcionario *f);

/* Marca o bloco do offset como livre para reaproveitamento futuro. */
void func_arquivo_remover(ArquivoFuncionarios *af, long offset);

/* ---------------------------------------------------------------------------
 * Callbacks exigidos pela Arvore B+ generica (bplus.h) para a ChaveComposta
 * ------------------------------------------------------------------------- */
int   chave_comparar(const void *a, const void *b);
void  chave_escrever(const void *chave, unsigned char *buf);
void *chave_ler(const unsigned char *buf);
int   chave_tamanho(void);
void  chave_liberar(void *chave);
void  chave_imprimir(const void *chave);

/* Cria uma ChaveComposta alocada dinamicamente (para usar com a arvore). */
ChaveComposta *chave_criar(const char *nome, Data nascimento);

/* Compara apenas por nome (usado para localizar homonimos e busca por
 * intervalo alfabetico, ignorando a data). Retorna <0, 0, >0. */
int chave_comparar_nome(const char *nomeA, const char *nomeB);

/* Utilitarios de data */
void data_ler_teclado(const char *rotulo, Data *d);
void data_imprimir(Data d);
int  data_valida(Data d);
int  data_comparar(Data a, Data b); /* <0, 0, >0 */
void data_hoje(Data *d);

#endif /* FUNCIONARIO_H */
