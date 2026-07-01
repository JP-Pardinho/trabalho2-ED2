/*
    ESTRUTURA DA DADOS II - AVALIAÇÃO 2
    GRUPO: 6
    ALUNOS: 
        - João Pedro Pardinho Rodrigues
        - Nicolas Leal Espindula
        - Gabriel dos Santos Lima
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bplus.h"
#include "funcionario.h"

#define ARQ_INDICE "rh_indice.dat"
#define ARQ_DADOS  "rh_funcionarios.dat"

/* ---------------------------------------------------------------------------
 * Utilitarios de entrada
 * ------------------------------------------------------------------------- */

static void ler_linha(const char *rotulo, char *destino, int tam_max) {
    printf("%s", rotulo);
    if (fgets(destino, tam_max, stdin)) {
        size_t len = strlen(destino);
        if (len > 0 && destino[len - 1] == '\n') destino[len - 1] = '\0';
    }
}

static int ler_inteiro(const char *rotulo) {
    char linha[32];
    int valor;
    while (1) {
        printf("%s", rotulo);
        if (fgets(linha, sizeof(linha), stdin) && sscanf(linha, "%d", &valor) == 1) {
            return valor;
        }
        printf("Entrada invalida.\n");
    }
}

static double ler_double(const char *rotulo) {
    char linha[32];
    double valor;
    while (1) {
        printf("%s", rotulo);
        if (fgets(linha, sizeof(linha), stdin) && sscanf(linha, "%lf", &valor) == 1) {
            return valor;
        }
        printf("Entrada invalida.\n");
    }
}

static char ler_sim_nao(const char *rotulo) {
    char linha[16];
    while (1) {
        printf("%s (S/N): ", rotulo);
        if (fgets(linha, sizeof(linha), stdin)) {
            char c = (char)toupper((unsigned char)linha[0]);
            if (c == 'S' || c == 'N') return c;
        }
        printf("Responda com S ou N.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Contexto usado pelas buscas por intervalo / percursos (via callback)
 * ------------------------------------------------------------------------- */

typedef struct {
    ArquivoFuncionarios *af;
    int contador;
} CtxListagem;

static void callback_listar_intervalo(const void *chave_generica, long offset_dado, void *ctx_generico) {
    const ChaveComposta *chave = (const ChaveComposta *)chave_generica;
    CtxListagem *ctx = (CtxListagem *)ctx_generico;
    Funcionario f;
    func_arquivo_ler(ctx->af, offset_dado, &f);
    ctx->contador++;
    printf("%3d) %-40s  Nasc: ", ctx->contador, chave->nome);
    data_imprimir(chave->data_nascimento);
    printf("  [%s]\n", f.ativo ? "Ativo" : "Inativo");
}

/* ---------------------------------------------------------------------------
 * Estrutura auxiliar: lista de homonimos encontrados numa busca por nome
 * ------------------------------------------------------------------------- */

#define MAX_HOMONIMOS 200

typedef struct {
    ChaveComposta chaves[MAX_HOMONIMOS];
    long offsets[MAX_HOMONIMOS];
    int total;
    char nome_buscado[TAM_NOME];
} CtxHomonimos;

static void callback_coletar_homonimos(const void *chave_generica, long offset_dado, void *ctx_generico) {
    const ChaveComposta *chave = (const ChaveComposta *)chave_generica;
    CtxHomonimos *ctx = (CtxHomonimos *)ctx_generico;
    if (chave_comparar_nome(chave->nome, ctx->nome_buscado) == 0) {
        if (ctx->total < MAX_HOMONIMOS) {
            ctx->chaves[ctx->total] = *chave;
            ctx->offsets[ctx->total] = offset_dado;
            ctx->total++;
        }
    }
}

/* Varre a arvore inteira coletando todos os registros cujo NOME (ignorando
 * data de nascimento) seja igual ao nome buscado. Usamos busca por
 * intervalo com limites um pouco antes/depois do nome para restringir a
 * varredura, aproveitando que a arvore esta ordenada primariamente por nome. */
static void buscar_homonimos(BPlusTree *arvore, const char *nome, CtxHomonimos *ctx) {
    ctx->total = 0;
    strncpy(ctx->nome_buscado, nome, TAM_NOME - 1);
    ctx->nome_buscado[TAM_NOME - 1] = '\0';

    /* limites artificiais: (nome + '\0'-1 truque) nao é trivial com strings,
     * entao construimos chaveA com data minima "antes de tudo" e chaveB com
     * data maxima "depois de tudo", e usamos um nome imediatamente anterior/
     * posterior para abranger todas as datas do mesmo nome dentro do
     * intervalo aberto exigido pela API. Como a comparacao de chave usa
     * primeiro o nome, isso restringe corretamente a varredura ao mesmo nome. */
    ChaveComposta chaveA, chaveB;
    strncpy(chaveA.nome, nome, TAM_NOME - 1); chaveA.nome[TAM_NOME - 1] = '\0';
    chaveA.data_nascimento.dia = 0; chaveA.data_nascimento.mes = 0; chaveA.data_nascimento.ano = 0;

    strncpy(chaveB.nome, nome, TAM_NOME - 1); chaveB.nome[TAM_NOME - 1] = '\0';
    chaveB.data_nascimento.dia = 32; chaveB.data_nascimento.mes = 13; chaveB.data_nascimento.ano = 9999;

    bplus_buscar_intervalo(arvore, &chaveA, &chaveB, callback_coletar_homonimos, ctx);
}

/* Exibe a lista de homonimos e pede ao usuario que escolha pela data de
 * nascimento. Retorna o indice escolhido em ctx->chaves/offsets, ou -1 se
 * nao encontrado / cancelado. */
static int desempatar_por_data(CtxHomonimos *ctx) {
    printf("\nForam encontrados %d registro(s) com o nome '%s':\n", ctx->total, ctx->nome_buscado);
    for (int i = 0; i < ctx->total; i++) {
        printf("  %d) %-40s Nasc: ", i + 1, ctx->chaves[i].nome);
        data_imprimir(ctx->chaves[i].data_nascimento);
        printf("\n");
    }
    Data d;
    data_ler_teclado("Informe a data de nascimento para desempate", &d);
    for (int i = 0; i < ctx->total; i++) {
        if (data_comparar(ctx->chaves[i].data_nascimento, d) == 0) {
            return i;
        }
    }
    printf("Nenhum registro corresponde a essa data de nascimento.\n");
    return -1;
}

/* ---------------------------------------------------------------------------
 * Exibicao de ficha do funcionario
 * ------------------------------------------------------------------------- */

static void exibir_funcionario_completo(const Funcionario *f) {
    printf("\n----------------------------------------------------\n");
    printf("Nome ............: %s\n", f->nome);
    printf("Data Nascimento .: "); data_imprimir(f->data_nascimento); printf("\n");
    printf("Nome da Mae .....: %s\n", f->nome_mae);
    printf("Nome do Pai .....: %s\n", f->nome_pai);
    printf("Endereco ........: %s\n", f->endereco);
    printf("Telefone ........: %s\n", f->telefone);
    printf("Data Contratacao : "); data_imprimir(f->data_contratacao); printf("\n");
    printf("Status ..........: %s\n", f->ativo ? "Ativo" : "Inativo");
    if (!f->ativo) {
        printf("Data Desligamento: "); data_imprimir(f->data_desligamento); printf("\n");
    }
    printf("Salario Base ....: %.2f\n", f->salario_base);
    printf("Historico de Pagamentos (%d registro(s)):\n", f->n_pagamentos);
    for (int i = 0; i < f->n_pagamentos; i++) {
        printf("   - "); data_imprimir(f->historico[i].data_pagamento);
        printf("  R$ %.2f\n", f->historico[i].valor);
    }
    printf("----------------------------------------------------\n");
}

static void exibir_funcionario_sem_pagamentos(const Funcionario *f) {
    printf("\n----------------------------------------------------\n");
    printf("Nome ............: %s\n", f->nome);
    printf("Data Nascimento .: "); data_imprimir(f->data_nascimento); printf("\n");
    printf("Nome da Mae .....: %s\n", f->nome_mae);
    printf("Nome do Pai .....: %s\n", f->nome_pai);
    printf("Endereco ........: %s\n", f->endereco);
    printf("Telefone ........: %s\n", f->telefone);
    printf("Data Contratacao : "); data_imprimir(f->data_contratacao); printf("\n");
    printf("Status ..........: %s\n", f->ativo ? "Ativo" : "Inativo");
    if (!f->ativo) {
        printf("Data Desligamento: "); data_imprimir(f->data_desligamento); printf("\n");
    }
    printf("Salario Base ....: %.2f\n", f->salario_base);
    printf("----------------------------------------------------\n");
}

/* ---------------------------------------------------------------------------
 * Cadastro de novos dados de funcionario via teclado
 * ------------------------------------------------------------------------- */

static void ler_dados_cadastrais(Funcionario *f, const char *nome, Data nascimento) {
    memset(f, 0, sizeof(Funcionario));
    strncpy(f->nome, nome, TAM_NOME - 1);
    f->data_nascimento = nascimento;

    ler_linha("Nome da mae: ", f->nome_mae, TAM_NOME);
    ler_linha("Nome do pai: ", f->nome_pai, TAM_NOME);
    ler_linha("Endereco residencial: ", f->endereco, TAM_ENDERECO);
    ler_linha("Telefone: ", f->telefone, TAM_TELEFONE);

    data_ler_teclado("Data de contratacao", &f->data_contratacao);

    char status = ler_sim_nao("Funcionario esta ATIVO?");
    f->ativo = (status == 'S') ? 1 : 0;
    if (!f->ativo) {
        data_ler_teclado("Data de desligamento", &f->data_desligamento);
    }

    f->salario_base = ler_double("Salario base (para geracao dos pagamentos mensais): ");
    f->n_pagamentos = 0; /* historico inicializado vazio, conforme especificacao */
}

/* ---------------------------------------------------------------------------
 * OPCAO 1: Inserir Funcionario
 * ------------------------------------------------------------------------- */

static void op_inserir(BPlusTree *arvore, ArquivoFuncionarios *af) {
    char nome[TAM_NOME];
    Data nascimento;

    printf("\n--- Inserir Funcionario ---\n");
    ler_linha("Nome: ", nome, TAM_NOME);
    data_ler_teclado("Data de nascimento", &nascimento);

    ChaveComposta chave;
    strncpy(chave.nome, nome, TAM_NOME - 1);
    chave.nome[TAM_NOME - 1] = '\0';
    chave.data_nascimento = nascimento;

    long offset_existente;
    if (bplus_buscar(arvore, &chave, &offset_existente)) {
        /* combinacao (nome, data) ja existe */
        Funcionario existente;
        func_arquivo_ler(af, offset_existente, &existente);
        printf("\nJa existe um funcionario com esse nome e data de nascimento:\n");
        exibir_funcionario_completo(&existente);

        char resp = ler_sim_nao("Deseja ATUALIZAR o cadastro deste funcionario?");
        if (resp == 'S') {
            Funcionario atualizado;
            int n_pag_antigo = existente.n_pagamentos;
            Pagamento hist_antigo[MAX_HISTORICO];
            memcpy(hist_antigo, existente.historico, sizeof(hist_antigo));

            ler_dados_cadastrais(&atualizado, nome, nascimento);

            /* preserva o historico de pagamentos existente (nao deve ser
             * apagado numa atualizacao cadastral) */
            atualizado.n_pagamentos = n_pag_antigo;
            memcpy(atualizado.historico, hist_antigo, sizeof(hist_antigo));

            func_arquivo_atualizar(af, offset_existente, &atualizado);
            printf("Cadastro atualizado com sucesso.\n");
        } else {
            printf("Operacao cancelada. Nenhum dado foi alterado.\n");
        }
        return;
    }

    /* nao existe: cadastra do zero */
    Funcionario novo;
    ler_dados_cadastrais(&novo, nome, nascimento);

    long offset_dado = func_arquivo_inserir(af, &novo);
    int ok = bplus_inserir(arvore, &chave, offset_dado);
    if (ok) {
        printf("Funcionario inserido com sucesso (offset de dados = %ld).\n", offset_dado);
    } else {
        /* nao deveria acontecer, pois ja checamos duplicidade acima */
        printf("ERRO: nao foi possivel inserir (chave duplicada inesperada).\n");
        func_arquivo_remover(af, offset_dado);
    }
}

/* ---------------------------------------------------------------------------
 * Funcao comum: localizar funcionario por nome, tratando homonimos.
 * Retorna 1 se um registro foi selecionado (preenche chave_saida/offset_saida),
 * 0 caso contrario (nao encontrado ou cancelado).
 * ------------------------------------------------------------------------- */

static int localizar_por_nome(BPlusTree *arvore, const char *nome,
                               ChaveComposta *chave_saida, long *offset_saida) {
    CtxHomonimos ctx;
    buscar_homonimos(arvore, nome, &ctx);

    if (ctx.total == 0) {
        printf("Nenhum funcionario encontrado com o nome '%s'.\n", nome);
        return 0;
    }
    if (ctx.total == 1) {
        *chave_saida = ctx.chaves[0];
        *offset_saida = ctx.offsets[0];
        return 1;
    }
    int idx = desempatar_por_data(&ctx);
    if (idx < 0) return 0;
    *chave_saida = ctx.chaves[idx];
    *offset_saida = ctx.offsets[idx];
    return 1;
}

/* ---------------------------------------------------------------------------
 * OPCAO 2: Buscar Funcionario
 * ------------------------------------------------------------------------- */

static void op_buscar(BPlusTree *arvore, ArquivoFuncionarios *af) {
    char nome[TAM_NOME];
    printf("\n--- Buscar Funcionario ---\n");
    ler_linha("Nome do funcionario: ", nome, TAM_NOME);

    ChaveComposta chave;
    long offset;
    if (!localizar_por_nome(arvore, nome, &chave, &offset)) return;

    Funcionario f;
    func_arquivo_ler(af, offset, &f);
    printf("\nFicha cadastral completa:\n");
    exibir_funcionario_completo(&f);
}

/* ---------------------------------------------------------------------------
 * OPCAO 3: Excluir Funcionario
 * ------------------------------------------------------------------------- */

static void op_excluir(BPlusTree *arvore, ArquivoFuncionarios *af) {
    char nome[TAM_NOME];
    printf("\n--- Excluir Funcionario ---\n");
    ler_linha("Nome do funcionario: ", nome, TAM_NOME);

    ChaveComposta chave;
    long offset;
    if (!localizar_por_nome(arvore, nome, &chave, &offset)) return;

    Funcionario f;
    func_arquivo_ler(af, offset, &f);

    printf("\nDados do funcionario a ser excluido:\n");
    exibir_funcionario_sem_pagamentos(&f);

    char resp = ler_sim_nao("Confirma a exclusao deste funcionario?");
    if (resp != 'S') {
        printf("Exclusao cancelada.\n");
        return;
    }

    int removido = bplus_remover(arvore, &chave);
    if (removido) {
        func_arquivo_remover(af, offset);
        printf("Funcionario removido com sucesso.\n");
    } else {
        printf("ERRO: nao foi possivel remover o registro da arvore.\n");
    }
}

/* ---------------------------------------------------------------------------
 * OPCAO 4: Listagem por Intervalo
 * ------------------------------------------------------------------------- */

static void op_listar_intervalo(BPlusTree *arvore, ArquivoFuncionarios *af) {
    char nomeA[TAM_NOME], nomeB[TAM_NOME];
    printf("\n--- Listagem por Intervalo (Nome A, Nome B) ---\n");
    ler_linha("Nome A (limite inferior, exclusivo): ", nomeA, TAM_NOME);
    ler_linha("Nome B (limite superior, exclusivo): ", nomeB, TAM_NOME);

    ChaveComposta chaveA, chaveB;
    strncpy(chaveA.nome, nomeA, TAM_NOME - 1); chaveA.nome[TAM_NOME - 1] = '\0';
    chaveA.data_nascimento.dia = 32; chaveA.data_nascimento.mes = 13; chaveA.data_nascimento.ano = 9999;
    /* usamos data "maxima" em A para garantir que qualquer data do mesmo
     * nome de A fique FORA do intervalo (exclusivo), conforme pedido */

    strncpy(chaveB.nome, nomeB, TAM_NOME - 1); chaveB.nome[TAM_NOME - 1] = '\0';
    chaveB.data_nascimento.dia = 0; chaveB.data_nascimento.mes = 0; chaveB.data_nascimento.ano = 0;
    /* data "minima" em B garante que registros do proprio nome B tambem
     * fiquem fora do intervalo (exclusivo) */

    printf("\nFuncionarios com nome entre '%s' e '%s' (intervalo aberto):\n", nomeA, nomeB);
    CtxListagem ctx = { af, 0 };
    bplus_buscar_intervalo(arvore, &chaveA, &chaveB, callback_listar_intervalo, &ctx);

    if (ctx.contador == 0) {
        printf("Nenhum funcionario encontrado nesse intervalo.\n");
    } else {
        printf("Total: %d funcionario(s).\n", ctx.contador);
    }
}

/* ---------------------------------------------------------------------------
 * OPCAO 5: Exibir Estrutura do Indice
 * ------------------------------------------------------------------------- */

static void op_exibir_estrutura(BPlusTree *arvore) {
    printf("\n--- Estrutura da Arvore B+ (Indice em Disco) ---\n");
    bplus_imprimir_estrutura(arvore);
}

/* ---------------------------------------------------------------------------
 * Menu principal
 * ------------------------------------------------------------------------- */

static void exibir_menu(void) {
    printf("\n============================================\n");
    printf(" SISTEMA DE GESTAO DE RH E FOLHA DE PAGAMENTO\n");
    printf("============================================\n");
    printf("1. Inserir Funcionario\n");
    printf("2. Buscar Funcionario\n");
    printf("3. Excluir Funcionario\n");
    printf("4. Listagem por Intervalo (Nome A, Nome B)\n");
    printf("5. Exibir Estrutura do Indice (Arvore B+)\n");
    printf("6. Sair\n");
    printf("============================================\n");
}

int main(void) {
    BPlusTree *arvore = bplus_abrir(ARQ_INDICE,
                                     chave_comparar,
                                     chave_escrever,
                                     chave_ler,
                                     chave_tamanho,
                                     chave_liberar,
                                     chave_imprimir);
    if (!arvore) {
        fprintf(stderr, "Falha ao abrir/criar o arquivo de indice.\n");
        return 1;
    }

    ArquivoFuncionarios *af = func_arquivo_abrir(ARQ_DADOS);
    if (!af) {
        fprintf(stderr, "Falha ao abrir/criar o arquivo de dados.\n");
        bplus_fechar(arvore);
        return 1;
    }

    printf("Sistema de RH carregado. Indice: %s | Dados: %s\n", ARQ_INDICE, ARQ_DADOS);

    int opcao;
    do {
        exibir_menu();
        opcao = ler_inteiro("Escolha uma opcao: ");
        switch (opcao) {
            case 1: op_inserir(arvore, af); break;
            case 2: op_buscar(arvore, af); break;
            case 3: op_excluir(arvore, af); break;
            case 4: op_listar_intervalo(arvore, af); break;
            case 5: op_exibir_estrutura(arvore); break;
            case 6:
                printf("Encerrando o sistema e garantindo integridade dos arquivos...\n");
                break;
            default:
                printf("Opcao invalida.\n");
        }
    } while (opcao != 6);

    /* fechamento seguro: grava cabecalhos e libera recursos */
    func_arquivo_fechar(af);
    bplus_fechar(arvore);

    printf("Sistema encerrado com sucesso.\n");
    return 0;
}
