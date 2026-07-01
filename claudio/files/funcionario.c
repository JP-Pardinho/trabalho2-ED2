/*
 * funcionario.c
 * Implementação das funções que gravam e leem os registros de
 * funcionário no arquivo de dados em disco.
 */

#include "funcionario.h"

FILE* abrir_arquivo_dados(const char *caminho) {
    FILE *arq = fopen(caminho, "rb+");
    if (arq == NULL) {
        /* Arquivo ainda não existe, cria um novo */
        arq = fopen(caminho, "wb+");
    }
    return arq;
}

long gravar_funcionario(FILE *arq, const Funcionario *f) {
    long endereco;
    fseek(arq, 0, SEEK_END);
    endereco = ftell(arq);

    fwrite(f, sizeof(Funcionario), 1, arq);
    fflush(arq);

    return endereco;
}

void atualizar_funcionario(FILE *arq, long endereco, const Funcionario *f) {
    fseek(arq, endereco, SEEK_SET);
    fwrite(f, sizeof(Funcionario), 1, arq);
    fflush(arq);
}

Funcionario ler_funcionario(FILE *arq, long endereco) {
    Funcionario f;
    fseek(arq, endereco, SEEK_SET);
    fread(&f, sizeof(Funcionario), 1, arq);
    return f;
}

void imprimir_ficha_resumida(const Funcionario *f) {
    printf("Nome.............: %s\n", f->nome);
    printf("Nascimento.......: %02d/%02d/%04d\n", f->nascimento.dia, f->nascimento.mes, f->nascimento.ano);
    printf("Mae..............: %s\n", f->nome_mae);
    printf("Pai..............: %s\n", f->nome_pai);
    printf("Endereco.........: %s\n", f->endereco);
    printf("Telefone.........: %s\n", f->telefone);
    printf("Contratacao......: %02d/%02d/%04d\n", f->data_contratacao.dia, f->data_contratacao.mes, f->data_contratacao.ano);
    printf("Status...........: %s\n", f->ativo ? "Ativo" : "Inativo");
    if (!f->ativo) {
        printf("Desligamento.....: %02d/%02d/%04d\n", f->data_desligamento.dia, f->data_desligamento.mes, f->data_desligamento.ano);
    }
}

void imprimir_ficha_completa(const Funcionario *f) {
    imprimir_ficha_resumida(f);

    printf("Historico de pagamentos (%d registrados):\n", f->qtd_pagamentos);
    if (f->qtd_pagamentos == 0) {
        printf("   (nenhum pagamento registrado ainda)\n");
    }
    for (int i = 0; i < f->qtd_pagamentos; i++) {
        printf("   %02d/%02d/%04d - R$ %.2f\n",
               f->historico[i].data_pagamento.dia,
               f->historico[i].data_pagamento.mes,
               f->historico[i].data_pagamento.ano,
               f->historico[i].valor);
    }
}
