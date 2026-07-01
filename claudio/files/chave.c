/*
 * chave.c
 * Implementação das funções de callback da chave composta (Nome + Nascimento).
 * São essas funções que "ensinam" a Árvore B+ genérica a lidar com o
 * nosso tipo de dado, sem que a árvore precise conhecer a struct.
 */

#include "chave.h"

ChaveFuncionario* criar_chave(const char *nome, int dia, int mes, int ano) {
    ChaveFuncionario *k = (ChaveFuncionario *) malloc(sizeof(ChaveFuncionario));
    memset(k, 0, sizeof(ChaveFuncionario));
    strncpy(k->nome, nome, TAM_NOME - 1);
    k->nascimento.dia = dia;
    k->nascimento.mes = mes;
    k->nascimento.ano = ano;
    return k;
}

/* Transforma a data em um número comparável, tipo AAAAMMDD */
static long data_para_numero(Data d) {
    return (long) d.ano * 10000L + (long) d.mes * 100L + (long) d.dia;
}

/* Compara primeiro pelo nome, e em caso de empate, pela data de nascimento */
int chave_comparar(const void *chaveA, const void *chaveB) {
    const ChaveFuncionario *a = (const ChaveFuncionario *) chaveA;
    const ChaveFuncionario *b = (const ChaveFuncionario *) chaveB;

    int cmp_nome = strcmp(a->nome, b->nome);
    if (cmp_nome != 0) {
        return cmp_nome;
    }

    long da = data_para_numero(a->nascimento);
    long db = data_para_numero(b->nascimento);

    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/* Serializa a chave em bytes para gravar no arquivo de índice em disco */
void chave_serializar(const void *chave, unsigned char *buffer) {
    const ChaveFuncionario *k = (const ChaveFuncionario *) chave;
    unsigned char *cursor = buffer;

    memcpy(cursor, k->nome, TAM_NOME);
    cursor += TAM_NOME;
    memcpy(cursor, &k->nascimento, sizeof(Data));
}

/* Reconstrói a chave a partir dos bytes lidos do disco */
void* chave_deserializar(const unsigned char *buffer) {
    ChaveFuncionario *k = (ChaveFuncionario *) malloc(sizeof(ChaveFuncionario));
    const unsigned char *cursor = buffer;

    memcpy(k->nome, cursor, TAM_NOME);
    cursor += TAM_NOME;
    memcpy(&k->nascimento, cursor, sizeof(Data));

    return k;
}

/* Tamanho em bytes que a chave ocupa quando serializada (deve caber em TAM_MAX_CHAVE) */
int chave_tamanho(void) {
    return TAM_NOME + sizeof(Data);
}

void chave_liberar(void *chave) {
    free(chave);
}

void chave_imprimir(const void *chave) {
    const ChaveFuncionario *k = (const ChaveFuncionario *) chave;
    printf("%s (%02d/%02d/%04d)", k->nome, k->nascimento.dia, k->nascimento.mes, k->nascimento.ano);
}
