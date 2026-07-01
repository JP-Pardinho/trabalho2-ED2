#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Bplus.h"
#include "funcionario.h"

#define ARQ_INDICE "indice.bin"
#define ARQ_DADOS  "dados.bin"
#define MAX_HOMONIMOS 100

typedef struct {
    ChaveFuncionario chaves[MAX_HOMONIMOS];
    long enderecos[MAX_HOMONIMOS];
    int qtd;
} ListaResultados;

// ============================================================================
// CALLBACKS DA CHAVE MOVIDOS AQUI
// ============================================================================
ChaveFuncionario* criarChave(const char *nome, int dia, int mes, int ano) {
    ChaveFuncionario *k = (ChaveFuncionario *) malloc(sizeof(ChaveFuncionario));
    memset(k, 0, sizeof(ChaveFuncionario));
    strncpy(k->nome, nome, TAM_NOME - 1);
    k->nascimento.dia = dia;
    k->nascimento.mes = mes;
    k->nascimento.ano = ano;
    return k;
}

long dataParaNumero(Data d) {
    return (long) d.ano * 10000L + (long) d.mes * 100L + (long) d.dia;
}

int compararChave(const void *chaveA, const void *chaveB) {
    const ChaveFuncionario *a = (const ChaveFuncionario *) chaveA;
    const ChaveFuncionario *b = (const ChaveFuncionario *) chaveB;
    int cmpNome = strcmp(a->nome, b->nome);
    if (cmpNome != 0) return cmpNome;
    
    long da = dataParaNumero(a->nascimento);
    long db = dataParaNumero(b->nascimento);
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

void serializarChave(const void *chave, unsigned char *buffer) {
    const ChaveFuncionario *k = (const ChaveFuncionario *) chave;
    unsigned char *cursor = buffer;
    memcpy(cursor, k->nome, TAM_NOME);
    cursor += TAM_NOME;
    memcpy(cursor, &k->nascimento, sizeof(Data));
}

void* deserializarChave(const unsigned char *buffer) {
    ChaveFuncionario *k = (ChaveFuncionario *) malloc(sizeof(ChaveFuncionario));
    const unsigned char *cursor = buffer;
    memcpy(k->nome, cursor, TAM_NOME);
    cursor += TAM_NOME;
    memcpy(&k->nascimento, cursor, sizeof(Data));
    return k;
}

int tamanhoChave(void) {
    return TAM_NOME + sizeof(Data);
}

void liberarChave(void *chave) {
    free(chave);
}

void imprimirChave(const void *chave) {
    const ChaveFuncionario *k = (const ChaveFuncionario *) chave;
    printf("%s (%02d/%02d/%04d)", k->nome, k->nascimento.dia, k->nascimento.mes, k->nascimento.ano);
}

// ============================================================================
// OPÇÕES DO SISTEMA E AUXILIARES
// ============================================================================
void lerLinha(char *destino, int tamanho) {
    if (fgets(destino, tamanho, stdin) != NULL) {
        destino[strcspn(destino, "\n")] = '\0';
    }
}

int lerInteiro(void) {
    char buffer[32];
    lerLinha(buffer, sizeof(buffer));
    return atoi(buffer);
}

void lerData(const char *rotulo, Data *d) {
    printf("%s (dia mes ano): ", rotulo);
    char buffer[32];
    lerLinha(buffer, sizeof(buffer));
    sscanf(buffer, "%d %d %d", &d->dia, &d->mes, &d->ano);
}

void coletarResultado(const void *chave, long endereco, void *contexto) {
    ListaResultados *lista = (ListaResultados *) contexto;
    if (lista->qtd < MAX_HOMONIMOS) {
        const ChaveFuncionario *k = (const ChaveFuncionario *) chave;
        lista->chaves[lista->qtd] = *k;
        lista->enderecos[lista->qtd] = endereco;
        lista->qtd++;
    }
}

void buscarTodosPorNome(ArvoreBPlus *arv, const char *nome, ListaResultados *lista) {
    lista->qtd = 0;
    ChaveFuncionario *chaveA = criarChave(nome, 0, 0, 0);
    ChaveFuncionario *chaveB = criarChave(nome, 31, 12, 9999);
    buscarIntervalo(arv, chaveA, chaveB, coletarResultado, lista);
    free(chaveA);
    free(chaveB);
}

int escolherHomonimo(const ListaResultados *lista) {
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
    lerLinha(buffer, sizeof(buffer));
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

void opcaoInserir(ArvoreBPlus *arv, FILE *dados) {
    char nome[TAM_NOME];
    Data nascimento;
    
    printf("\n--- Inserir Funcionario ---\n");
    printf("Nome: ");
    lerLinha(nome, sizeof(nome));
    lerData("Data de nascimento", &nascimento);
    
    ChaveFuncionario *chave = criarChave(nome, nascimento.dia, nascimento.mes, nascimento.ano);
    long endereco;
    bool jaExiste = buscarChave(arv, chave, &endereco);

    if (jaExiste) {
        Funcionario f = lerFuncionario(dados, endereco);
        printf("\nJa existe um funcionario com esse nome e data de nascimento:\n");
        imprimirFichaCompleta(&f);
        
        printf("\nDeseja atualizar o cadastro? (s/n): ");
        char resp[8];
        lerLinha(resp, sizeof(resp));
        
        if (resp[0] == 's' || resp[0] == 'S') {
            printf("Nome da mae: ");
            lerLinha(f.nomeMae, sizeof(f.nomeMae));
            printf("Nome do pai: ");
            lerLinha(f.nomePai, sizeof(f.nomePai));
            printf("Endereco: ");
            lerLinha(f.endereco, sizeof(f.endereco));
            printf("Telefone: ");
            lerLinha(f.telefone, sizeof(f.telefone));
            lerData("Data de contratacao", &f.dataContratacao);
            printf("Funcionario ativo? (1-Sim / 0-Nao): ");
            f.ativo = lerInteiro();
            if (!f.ativo) lerData("Data de desligamento", &f.dataDesligamento);
            
            atualizarFuncionario(dados, endereco, &f);
            printf("Cadastro atualizado com sucesso!\n");
        } else {
            printf("Operacao cancelada.\n");
        }
        free(chave);
        return;
    }

    Funcionario f;
    memset(&f, 0, sizeof(Funcionario));
    strncpy(f.nome, nome, TAM_NOME - 1);
    f.nascimento = nascimento;
    
    printf("Nome da mae: ");
    lerLinha(f.nomeMae, sizeof(f.nomeMae));
    printf("Nome do pai: ");
    lerLinha(f.nomePai, sizeof(f.nomePai));
    printf("Endereco: ");
    lerLinha(f.endereco, sizeof(f.endereco));
    printf("Telefone: ");
    lerLinha(f.telefone, sizeof(f.telefone));
    lerData("Data de contratacao", &f.dataContratacao);
    f.ativo = true;
    f.qtdPagamentos = 0; 
    
    long novoEndereco = gravarFuncionario(dados, &f);
    inserirChave(arv, chave, novoEndereco);
    printf("Funcionario cadastrado com sucesso!\n");
    free(chave);
}

void opcaoBuscar(ArvoreBPlus *arv, FILE *dados) {
    char nome[TAM_NOME];
    printf("\n--- Buscar Funcionario ---\n");
    printf("Nome: ");
    lerLinha(nome, sizeof(nome));
    
    ListaResultados lista;
    buscarTodosPorNome(arv, nome, &lista);
    
    if (lista.qtd == 0) {
        printf("Nenhum funcionario encontrado com esse nome.\n");
        return;
    }
    
    int escolhido = 0;
    if (lista.qtd > 1) {
        escolhido = escolherHomonimo(&lista);
        if (escolhido == -1) {
            printf("Data de nascimento nao encontrada. Operacao cancelada.\n");
            return;
        }
    }
    Funcionario f = lerFuncionario(dados, lista.enderecos[escolhido]);
    printf("\n--- Ficha do Funcionario ---\n");
    imprimirFichaCompleta(&f);
}

void opcaoExcluir(ArvoreBPlus *arv, FILE *dados) {
    char nome[TAM_NOME];
    printf("\n--- Excluir Funcionario ---\n");
    printf("Nome: ");
    lerLinha(nome, sizeof(nome));
    
    ListaResultados lista;
    buscarTodosPorNome(arv, nome, &lista);
    
    if (lista.qtd == 0) {
        printf("Nenhum funcionario encontrado com esse nome.\n");
        return;
    }
    
    int escolhido = 0;
    if (lista.qtd > 1) {
        escolhido = escolherHomonimo(&lista);
        if (escolhido == -1) {
            printf("Data de nascimento nao encontrada. Operacao cancelada.\n");
            return;
        }
    }
    Funcionario f = lerFuncionario(dados, lista.enderecos[escolhido]);
    printf("\nDados do funcionario selecionado:\n");
    imprimirFichaResumida(&f);
    
    printf("\nConfirma a exclusao? (s/n): ");
    char resp[8];
    lerLinha(resp, sizeof(resp));
    
    if (resp[0] == 's' || resp[0] == 'S') {
        ChaveFuncionario *chave = criarChave(f.nome, f.nascimento.dia, f.nascimento.mes, f.nascimento.ano);
        removerChave(arv, chave);
        free(chave);
        printf("Funcionario removido do indice com sucesso!\n");
    } else {
        printf("Exclusao cancelada.\n");
    }
}

typedef struct {
    FILE *dados;
    int contador;
} ContextoIntervalo;

void imprimirLinhaIntervalo(const void *chave, long endereco, void *contexto) {
    ContextoIntervalo *ctx = (ContextoIntervalo *) contexto;
    Funcionario f = lerFuncionario(ctx->dados, endereco);
    ctx->contador++;
    printf("  %d) %s - %02d/%02d/%04d\n", ctx->contador, f.nome,
           f.nascimento.dia, f.nascimento.mes, f.nascimento.ano);
}

void opcaoIntervalo(ArvoreBPlus *arv, FILE *dados) {
    char nomeA[TAM_NOME], nomeB[TAM_NOME];
    printf("\n--- Listagem por Intervalo ---\n");
    printf("Nome A (limite inicial): ");
    lerLinha(nomeA, sizeof(nomeA));
    printf("Nome B (limite final): ");
    lerLinha(nomeB, sizeof(nomeB));
    
    ChaveFuncionario *chaveA = criarChave(nomeA, 0, 0, 0);
    ChaveFuncionario *chaveB = criarChave(nomeB, 31, 12, 9999);
    ContextoIntervalo ctx = { dados, 0 };
    
    printf("\nFuncionarios entre \"%s\" e \"%s\":\n", nomeA, nomeB);
    buscarIntervalo(arv, chaveA, chaveB, imprimirLinhaIntervalo, &ctx);
    
    if (ctx.contador == 0) {
        printf("  (nenhum funcionario encontrado nesse intervalo)\n");
    }
    free(chaveA);
    free(chaveB);
}

void exibirMenu(void) {
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
    ArvoreBPlus *arvore = criarArvore(ARQ_INDICE, compararChave, serializarChave, deserializarChave, tamanhoChave, liberarChave, imprimirChave);
    
    FILE *dados = abrirArquivoDados(ARQ_DADOS);
    if (dados == NULL) {
        printf("Erro ao abrir o arquivo de dados.\n");
        return 1;
    }
    
    int opcao;
    do {
        exibirMenu();
        opcao = lerInteiro();
        
        switch (opcao) {
            case 1: opcaoInserir(arvore, dados); break;
            case 2: opcaoBuscar(arvore, dados); break;
            case 3: opcaoExcluir(arvore, dados); break;
            case 4: opcaoIntervalo(arvore, dados); break;
            case 5: imprimirEstruturaArvore(arvore); break;
            case 6: printf("Encerrando o sistema...\n"); break;
            default: printf("Opcao invalida!\n"); break;
        }
    } while (opcao != 6);

    fclose(dados);
    fecharArvore(arvore);
    return 0;
}