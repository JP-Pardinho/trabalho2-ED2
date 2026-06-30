#include "funcionarios.h"

// =======================================================
// Callback: Comparação Genérica Injetada na Árvore
// =======================================================
int comparar_chaves_funcionario(const void *a, const void *b) {
    // 1. Faz o cast dos ponteiros genéricos de volta para a nossa struct
    const ChaveFuncionario *chaveA = (const ChaveFuncionario *)a;
    const ChaveFuncionario *chaveB = (const ChaveFuncionario *)b;

    // 2. Critério Principal: Ordem alfabética do Nome
    int comp_nome = strcmp(chaveA->nome, chaveB->nome);
    if (comp_nome != 0) {
        return comp_nome; // Se os nomes são diferentes, o resultado já está decidido
    }

    // 3. Critério de Desempate: Data de Nascimento (Homónimos)
    // Comparamos do mais abrangente (Ano) para o mais específico (Dia)
    if (chaveA->nascimento.ano != chaveB->nascimento.ano) {
        return chaveA->nascimento.ano - chaveB->nascimento.ano;
    }
    
    if (chaveA->nascimento.mes != chaveB->nascimento.mes) {
        return chaveA->nascimento.mes - chaveB->nascimento.mes;
    }
    
    // Se o ano e o mês são iguais, a decisão final recai sobre o dia
    return chaveA->nascimento.dia - chaveB->nascimento.dia;
}

// =======================================================
// Callback: Impressão Textual para a Estrutura da Árvore
// =======================================================
void imprimir_chave_funcionario(void *chave) {
    ChaveFuncionario *c = (ChaveFuncionario *)chave;
    // Extrai o primeiro nome para não poluir a visualização da árvore
    char primeiro_nome[50];
    sscanf(c->nome, "%49s", primeiro_nome);
    
    // O requisito pede para exibir o nome e a data para distinguir os nós
    printf("%s(%02d/%02d/%04d)", primeiro_nome, c->nascimento.dia, c->nascimento.mes, c->nascimento.ano);
}