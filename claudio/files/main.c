/*
 * main.c
 * Sistema de Gestao de RH e Folha de Pagamento.
 * Usa a Arvore B+ generica (Bplus.h/Bplus.c) como indice em disco,
 * com chave composta Nome + Data de Nascimento (chave.h/chave.c).
 * Os dados completos de cada funcionario ficam no arquivo dados.bin
 * (funcionario.h/funcionario.c). A arvore guarda so o endereco do
 * registro dentro desse arquivo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Bplus.h"
#include "chave.h"
#include "funcionario.h"

#define ARQ_INDICE "indice.bin"
#define ARQ_DADOS  "dados.bin"
#define MAX_HOMONIMOS 100

/* Contexto usado para juntar os resultados de uma busca por intervalo */
typedef struct {
    ChaveFuncionario chaves[MAX_HOMONIMOS];
    long enderecos[MAX_HOMONIMOS];
    int qtd;
} ListaResultados;

/* ============================================================================
 * FUNCOES AUXILIARES DE LEITURA DO TECLADO
 * ========================================================================= */
static void ler_linha(char *destino, int tamanho) {
    if (fgets(destino, tamanho, stdin) != NULL) {
        /* remove o \n do final, se existir */
        destino[strcspn(destino, "\n")] = '\0';
    }
}

static int ler_inteiro(void) {
    char buffer[32];
    ler_linha(buffer, sizeof(buffer));
    return atoi(buffer);
}

static void ler_data(const char *rotulo, Data *d) {
    printf("%s (dia mes ano): ", rotulo);
    char buffer[32];
    ler_linha(buffer, sizeof(buffer));
    sscanf(buffer, "%d %d %d", &d->dia, &d->mes, &d->ano);
}

/* ============================================================================
 * CALLBACK USADO NA BUSCA POR INTERVALO (Arvore B+)
 * ========================================================================= */
static void coletar_resultado(const void *chave, long endereco, void *contexto) {
    ListaResultados *lista = (ListaResultados *) contexto;
    if (lista->qtd < MAX_HOMONIMOS) {
        const ChaveFuncionario *k = (const ChaveFuncionario *) chave;
        lista->chaves[lista->qtd] = *k;
        lista->enderecos[lista->qtd] = endereco;
        lista->qtd++;
    }
}

/* Busca todos os registros com um determinado nome (qualquer data de nascimento) */
static void buscar_todos_por_nome(ArvoreBPlus *arv, const char *nome, ListaResultados *lista) {
    lista->qtd = 0;

    ChaveFuncionario *chaveA = criar_chave(nome, 0, 0, 0);
    ChaveFuncionario *chaveB = criar_chave(nome, 31, 12, 9999);

    buscar_intervalo(arv, chaveA, chaveB, coletar_resultado, lista);

    free(chaveA);
    free(chaveB);
}

/*
 * Quando existe mais de um funcionario com o mesmo nome, pede ao usuario
 * que escolha a data de nascimento para desempatar. Devolve o indice
 * escolhido na lista, ou -1 se o usuario cancelar.
 */
static int escolher_homonimo(const ListaResultados *lista) {
    printf("\nForam encontrados %d funcionarios com esse nome:\n", lista->qtd);
    for (int i = 0; i < lista->qtd; i++) {
        printf("  [%d] %s - nascido em %02d/%02d/%04d\n", i + 1,
               lista->chaves[i].nome,
               lista->chaves[i].nascimento.dia,
               lista->chaves[i].nascimento.mes,
               lista->chaves[i].nascimento.ano);
    }
    printf("Digite a data de nascimento do funcionario desejado (dia mes ano): ");

    char buffer[32];
    ler_linha(buffer, sizeof(buffer));
    int dia, mes, ano;
    sscanf(buffer, "%d %d %d", &dia, &mes, &ano);

    for (int i = 0; i < lista->qtd; i++) {
        if (lista->chaves[i].nascimento.dia == dia &&
            lista->chaves[i].nascimento.mes == mes &&
            lista->chaves[i].nascimento.ano == ano) {
            return i;
        }
    }
    return -1;
}

/* ============================================================================
 * OPCOES DO MENU
 * ========================================================================= */
static void opcao_inserir(ArvoreBPlus *arv, FILE *dados) {
    char nome[TAM_NOME];
    Data nascimento;

    printf("\n--- Inserir Funcionario ---\n");
    printf("Nome: ");
    ler_linha(nome, sizeof(nome));
    ler_data("Data de nascimento", &nascimento);

    ChaveFuncionario *chave = criar_chave(nome, nascimento.dia, nascimento.mes, nascimento.ano);

    long endereco;
    bool ja_existe = buscar_chave(arv, chave, &endereco);

    if (ja_existe) {
        Funcionario f = ler_funcionario(dados, endereco);
        printf("\nJa existe um funcionario com esse nome e data de nascimento:\n");
        imprimir_ficha_completa(&f);

        printf("\nDeseja atualizar o cadastro? (s/n): ");
        char resp[8];
        ler_linha(resp, sizeof(resp));

        if (resp[0] == 's' || resp[0] == 'S') {
            printf("Nome da mae: ");
            ler_linha(f.nome_mae, sizeof(f.nome_mae));
            printf("Nome do pai: ");
            ler_linha(f.nome_pai, sizeof(f.nome_pai));
            printf("Endereco: ");
            ler_linha(f.endereco, sizeof(f.endereco));
            printf("Telefone: ");
            ler_linha(f.telefone, sizeof(f.telefone));
            ler_data("Data de contratacao", &f.data_contratacao);

            printf("Funcionario ativo? (1-Sim / 0-Nao): ");
            f.ativo = ler_inteiro();
            if (!f.ativo) {
                ler_data("Data de desligamento", &f.data_desligamento);
            }

            atualizar_funcionario(dados, endereco, &f);
            printf("Cadastro atualizado com sucesso!\n");
        } else {
            printf("Operacao cancelada.\n");
        }

        free(chave);
        return;
    }

    /* Funcionario novo: pede os demais dados */
    Funcionario f;
    memset(&f, 0, sizeof(Funcionario));
    strncpy(f.nome, nome, TAM_NOME - 1);
    f.nascimento = nascimento;

    printf("Nome da mae: ");
    ler_linha(f.nome_mae, sizeof(f.nome_mae));
    printf("Nome do pai: ");
    ler_linha(f.nome_pai, sizeof(f.nome_pai));
    printf("Endereco: ");
    ler_linha(f.endereco, sizeof(f.endereco));
    printf("Telefone: ");
    ler_linha(f.telefone, sizeof(f.telefone));
    ler_data("Data de contratacao", &f.data_contratacao);
    f.ativo = true;
    f.qtd_pagamentos = 0; /* historico comeca vazio */

    long novo_endereco = gravar_funcionario(dados, &f);
    inserir_chave(arv, chave, novo_endereco);

    printf("Funcionario cadastrado com sucesso!\n");
    free(chave);
}

static void opcao_buscar(ArvoreBPlus *arv, FILE *dados) {
    char nome[TAM_NOME];
    printf("\n--- Buscar Funcionario ---\n");
    printf("Nome: ");
    ler_linha(nome, sizeof(nome));

    ListaResultados lista;
    buscar_todos_por_nome(arv, nome, &lista);

    if (lista.qtd == 0) {
        printf("Nenhum funcionario encontrado com esse nome.\n");
        return;
    }

    int escolhido = 0;
    if (lista.qtd > 1) {
        escolhido = escolher_homonimo(&lista);
        if (escolhido == -1) {
            printf("Data de nascimento nao encontrada na lista. Operacao cancelada.\n");
            return;
        }
    }

    Funcionario f = ler_funcionario(dados, lista.enderecos[escolhido]);
    printf("\n--- Ficha do Funcionario ---\n");
    imprimir_ficha_completa(&f);
}

static void opcao_excluir(ArvoreBPlus *arv, FILE *dados) {
    char nome[TAM_NOME];
    printf("\n--- Excluir Funcionario ---\n");
    printf("Nome: ");
    ler_linha(nome, sizeof(nome));

    ListaResultados lista;
    buscar_todos_por_nome(arv, nome, &lista);

    if (lista.qtd == 0) {
        printf("Nenhum funcionario encontrado com esse nome.\n");
        return;
    }

    int escolhido = 0;
    if (lista.qtd > 1) {
        escolhido = escolher_homonimo(&lista);
        if (escolhido == -1) {
            printf("Data de nascimento nao encontrada na lista. Operacao cancelada.\n");
            return;
        }
    }

    Funcionario f = ler_funcionario(dados, lista.enderecos[escolhido]);
    printf("\nDados do funcionario selecionado:\n");
    imprimir_ficha_resumida(&f);

    printf("\nConfirma a exclusao? (s/n): ");
    char resp[8];
    ler_linha(resp, sizeof(resp));

    if (resp[0] == 's' || resp[0] == 'S') {
        ChaveFuncionario *chave = criar_chave(f.nome, f.nascimento.dia, f.nascimento.mes, f.nascimento.ano);
        remover_chave(arv, chave);
        free(chave);
        printf("Funcionario removido do indice com sucesso!\n");
    } else {
        printf("Exclusao cancelada.\n");
    }
}

/* Contexto usado na listagem por intervalo: precisa do arquivo de dados para imprimir */
typedef struct {
    FILE *dados;
    int contador;
} ContextoIntervalo;

static void imprimir_linha_intervalo(const void *chave, long endereco, void *contexto) {
    ContextoIntervalo *ctx = (ContextoIntervalo *) contexto;
    Funcionario f = ler_funcionario(ctx->dados, endereco);
    ctx->contador++;
    printf("  %d) %s - %02d/%02d/%04d\n", ctx->contador, f.nome,
           f.nascimento.dia, f.nascimento.mes, f.nascimento.ano);
}

static void opcao_intervalo(ArvoreBPlus *arv, FILE *dados) {
    char nomeA[TAM_NOME], nomeB[TAM_NOME];
    printf("\n--- Listagem por Intervalo ---\n");
    printf("Nome A (limite inicial): ");
    ler_linha(nomeA, sizeof(nomeA));
    printf("Nome B (limite final): ");
    ler_linha(nomeB, sizeof(nomeB));

    ChaveFuncionario *chaveA = criar_chave(nomeA, 0, 0, 0);
    ChaveFuncionario *chaveB = criar_chave(nomeB, 31, 12, 9999);

    ContextoIntervalo ctx = { dados, 0 };

    printf("\nFuncionarios entre \"%s\" e \"%s\":\n", nomeA, nomeB);
    buscar_intervalo(arv, chaveA, chaveB, imprimir_linha_intervalo, &ctx);

    if (ctx.contador == 0) {
        printf("  (nenhum funcionario encontrado nesse intervalo)\n");
    }

    free(chaveA);
    free(chaveB);
}

/* ============================================================================
 * MENU PRINCIPAL
 * ========================================================================= */
static void exibir_menu(void) {
    printf("\n=================================================\n");
    printf("   SISTEMA DE GESTAO DE RH E FOLHA DE PAGAMENTO\n");
    printf("=================================================\n");
    printf("1 - Inserir Funcionario\n");
    printf("2 - Buscar Funcionario\n");
    printf("3 - Excluir Funcionario\n");
    printf("4 - Listagem por Intervalo\n");
    printf("5 - Exibir Estrutura do Indice\n");
    printf("6 - Sair\n");
    printf("Escolha uma opcao: ");
}

int main(void) {
    ArvoreBPlus *arvore = criar_arvore(ARQ_INDICE,
                                        chave_comparar, chave_serializar,
                                        chave_deserializar, chave_tamanho,
                                        chave_liberar, chave_imprimir);

    FILE *dados = abrir_arquivo_dados(ARQ_DADOS);
    if (dados == NULL) {
        printf("Erro ao abrir o arquivo de dados.\n");
        return 1;
    }

    int opcao;
    do {
        exibir_menu();
        opcao = ler_inteiro();

        switch (opcao) {
            case 1: opcao_inserir(arvore, dados); break;
            case 2: opcao_buscar(arvore, dados); break;
            case 3: opcao_excluir(arvore, dados); break;
            case 4: opcao_intervalo(arvore, dados); break;
            case 5: imprimir_estrutura_arvore(arvore); break;
            case 6: printf("Encerrando o sistema...\n"); break;
            default: printf("Opcao invalida!\n"); break;
        }
    } while (opcao != 6);

    fclose(dados);
    fechar_arvore(arvore);
    return 0;
}
