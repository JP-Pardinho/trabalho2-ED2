/*
    ESTRUTURA DA DADOS II - AVALIAÇÃO 2
    PROFESSORA: Dra. Luciana Lee
    ALUNOS: 
        - 2023201331 | Gabriel dos Santos Lima
        - 2023201073 | João Pedro Pardinho Rodrigues
        - 2023200798 | Nicolas Leal Espindula
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Bplus.h"
#include "funcionario.h"

#define ARQ_INDICE "indice.bin"
#define ARQ_DADOS "dados.bin"
#define MAX_HOMONIMOS 100

typedef struct {
    ChaveFuncionario chaves[MAX_HOMONIMOS];
    long enderecos[MAX_HOMONIMOS];
    int qtd;
} ListaResultados;

// ============================================================================
// CALLBACKS DA CHAVE MOVIDOS AQUI
// ============================================================================
/**
 * @brief Cria e inicializa uma nova ChaveFuncionario.
 * 
 * @param nome (const char*) Nome do funcionário.
 * @param dia (int) Dia de nascimento.
 * @param mes (int) Mês de nascimento.
 * @param ano (int) Ano de nascimento.
 * @return (ChaveFuncionario*) Ponteiro para a chave alocada e inicializada.
 */
ChaveFuncionario *criarChave(const char *nome, int dia, int mes, int ano) {
    ChaveFuncionario *k = (ChaveFuncionario *)malloc(sizeof(ChaveFuncionario));
    memset(k, 0, sizeof(ChaveFuncionario));
    strncpy(k->nome, nome, 100 - 1);
    k->nascimento.dia = dia;
    k->nascimento.mes = mes;
    k->nascimento.ano = ano;
    return k;
}

/**
 * @brief Converte uma Data para um número (formato numérico de 8 dígitos) a fim de facilitar comparações.
 * 
 * @param d (Data) Estrutura contendo a data.
 * @return (long) A representação numérica da data (ex: 20231201 para 01/12/2023).
 */
long dataParaNumero(Data d) {
    return (long)d.ano * 10000L + (long)d.mes * 100L + (long)d.dia;
}

/**
 * @brief Compara duas chaves de funcionários por nome e em seguida pela data de nascimento.
 * 
 * @param chaveA (const void*) Primeira chave a ser comparada.
 * @param chaveB (const void*) Segunda chave a ser comparada.
 * @return (int) Menor que 0 se A < B, 0 se A == B, e maior que 0 se A > B.
 */
int compararChave(const void *chaveA, const void *chaveB) {
    const ChaveFuncionario *a = (const ChaveFuncionario *)chaveA;
    const ChaveFuncionario *b = (const ChaveFuncionario *)chaveB;
    int cmpNome = strcmp(a->nome, b->nome);
    if (cmpNome != 0)
        return cmpNome;

    long da = dataParaNumero(a->nascimento);
    long db = dataParaNumero(b->nascimento);
    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
}

/**
 * @brief Serializa (grava) uma chave em um buffer de bytes.
 * 
 * @param chave (const void*) Ponteiro para a chave original.
 * @param buffer (unsigned char*) O buffer de destino onde os bytes serão escritos.
 * @return (void) Não retorna valor.
 */
void GravarChaveFunc(const void *chave, unsigned char *buffer) {
    const ChaveFuncionario *k = (const ChaveFuncionario *)chave;
    unsigned char *cursor = buffer;
    memcpy(cursor, k->nome, 100);
    cursor += 100;
    memcpy(cursor, &k->nascimento, sizeof(Data));
}

/**
 * @brief Desserializa (lê) uma chave a partir de um buffer de bytes.
 * 
 * @param buffer (const unsigned char*) Buffer de bytes que contém os dados da chave.
 * @return (void*) Ponteiro genérico para a nova chave alocada.
 */
void *LerChaveFunc(const unsigned char *buffer) {
    ChaveFuncionario *k = (ChaveFuncionario *)malloc(sizeof(ChaveFuncionario));
    const unsigned char *cursor = buffer;
    memcpy(k->nome, cursor, 100);
    cursor += 100;
    memcpy(&k->nascimento, cursor, sizeof(Data));
    return k;
}

/**
 * @brief Retorna o tamanho em bytes ocupado pela chave (nome + data).
 * 
 * @return (int) Tamanho exato de uma chave em bytes.
 */
int tamanhoChave(void) {
    return 100 + sizeof(Data);
}

/**
 * @brief Libera da memória uma chave previamente alocada.
 * 
 * @param chave (void*) Ponteiro para a chave a ser liberada.
 * @return (void) Não retorna valor.
 */
void liberarChave(void *chave) {
    free(chave);
}

/**
 * @brief Imprime as informações de uma chave no terminal (usado em debugs e impressões da árvore).
 * 
 * @param chave (const void*) Ponteiro para a chave que será impressa.
 * @return (void) Não retorna valor.
 */
void imprimirChave(const void *chave) {
    const ChaveFuncionario *k = (const ChaveFuncionario *)chave;
    printf("%s (%02d/%02d/%04d)", k->nome, k->nascimento.dia, k->nascimento.mes, k->nascimento.ano);
}

// ============================================================================
// OPÇÕES DO SISTEMA E AUXILIARES
// ============================================================================
/**
 * @brief Lê uma linha de texto da entrada padrão, removendo a quebra de linha final.
 * 
 * @param destino (char*) Buffer onde a string será armazenada.
 * @param tamanho (int) Tamanho máximo do buffer.
 * @return (void) Não retorna valor.
 */
void lerLinha(char *destino, int tamanho) {
    if (fgets(destino, tamanho, stdin) != NULL)
    {
        destino[strcspn(destino, "\n")] = '\0';
    }
}

/**
 * @brief Lê um número inteiro da entrada padrão de forma segura.
 * 
 * @return (int) O número inteiro digitado.
 */
int lerInteiro(void) {
    char buffer[32];
    lerLinha(buffer, sizeof(buffer)); 
    return atoi(buffer);
}

/**
 * @brief Lê uma data fornecida pelo usuário na entrada padrão e a formata.
 * 
 * @param rotulo (const char*) Mensagem que será exibida para solicitar a data.
 * @param d (Data*) Ponteiro para a estrutura Data onde será armazenada a data lida.
 * @return (void) Não retorna valor.
 */
static void lerData(const char *rotulo, Data *d) {
    printf("%s (dia mes ano): ", rotulo);
    char buffer[64];
    lerLinha(buffer, sizeof(buffer));

    // Inicializa com 0 para evitar valores estranhos se o sscanf falhar
    d->dia = 0;
    d->mes = 0;
    d->ano = 0;

    // Tenta ler e verifica se leu 3 inteiros
    if (sscanf(buffer, "%d %d %d", &d->dia, &d->mes, &d->ano) != 3)
    {
        printf("Formato invalido! Use: dia mes ano\n");
    }
}

/**
 * @brief Callback utilizado pela busca em intervalo para coletar múltiplos resultados.
 * 
 * @param chave (const void*) Chave encontrada na busca.
 * @param endereco (long) Endereço do registro correspondente.
 * @param contexto (void*) Ponteiro para a estrutura ListaResultados onde serão guardados os dados.
 * @return (void) Não retorna valor.
 */
void coletarResultado(const void *chave, long endereco, void *contexto) {
    ListaResultados *lista = (ListaResultados *)contexto;
    if (lista->qtd < MAX_HOMONIMOS)
    {
        const ChaveFuncionario *k = (const ChaveFuncionario *)chave;
        lista->chaves[lista->qtd] = *k;
        lista->enderecos[lista->qtd] = endereco;
        lista->qtd++;
    }
}

/**
 * @brief Busca e coleta todos os funcionários com um determinado nome na árvore B+.
 * 
 * @param arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 * @param nome (const char*) Nome a ser buscado.
 * @param lista (ListaResultados*) Lista onde os resultados encontrados serão salvos.
 * @return (void) Não retorna valor.
 */
void buscarTodosPorNome(ArvoreBPlus *arv, const char *nome, ListaResultados *lista) {
    lista->qtd = 0;
    ChaveFuncionario *chaveA = criarChave(nome, 0, 0, 0);
    ChaveFuncionario *chaveB = criarChave(nome, 31, 12, 9999);
    buscarIntervalo(arv, chaveA, chaveB, coletarResultado, lista);
    free(chaveA);
    free(chaveB);
}

/**
 * @brief Solicita que o usuário escolha um entre vários homônimos retornados na busca.
 * 
 * @param lista (const ListaResultados*) Lista de resultados de funcionários homônimos.
 * @return (int) O índice da escolha feita pelo usuário na lista, ou -1 se não encontrar a data informada.
 */
int escolherHomonimo(const ListaResultados *lista) {
    printf("\nForam encontrados %d funcionarios com esse nome:\n", lista->qtd);
    for (int i = 0; i < lista->qtd; i++)
    {
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

    for (int i = 0; i < lista->qtd; i++)
    {
        if (lista->chaves[i].nascimento.dia == dia &&
            lista->chaves[i].nascimento.mes == mes &&
            lista->chaves[i].nascimento.ano == ano)
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Lida com a opção de inserção no menu, coletando os dados e inserindo no sistema.
 * 
 * @param arv (ArvoreBPlus*) Ponteiro para a árvore B+.
 * @param dados (FILE*) Ponteiro para o arquivo de dados.
 * @return (void) Não retorna valor.
 */
void opcaoInserir(ArvoreBPlus *arv, FILE *dados) {
    char nome[100];
    Data nascimento;

    printf("\n--- Inserir Funcionario ---\n");
    printf("Nome: ");
    lerLinha(nome, sizeof(nome));
    lerData("Data de nascimento", &nascimento);

    ChaveFuncionario *chave = criarChave(nome, nascimento.dia, nascimento.mes, nascimento.ano);
    long endereco;
    bool jaExiste = buscarChave(arv, chave, &endereco);

    if (jaExiste)
    {
        Funcionario f = lerFuncionario(dados, endereco);
        printf("\nJa existe um funcionario com esse nome e data de nascimento:\n");
        imprimirFichaCompleta(&f);

        printf("\nDeseja atualizar o cadastro? (s/n): ");
        char resp[8];
        lerLinha(resp, sizeof(resp));

        if (resp[0] == 's' || resp[0] == 'S')
        {
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
            if (!f.ativo)
                lerData("Data de desligamento", &f.dataDesligamento);

            atualizarFuncionario(dados, endereco, &f);
            printf("Cadastro atualizado com sucesso!\n");
        }
        else
        {
            printf("Operacao cancelada.\n");
        }
        free(chave);
        return;
    }

    Funcionario f;
    memset(&f, 0, sizeof(Funcionario));
    strncpy(f.nome, nome, 100 - 1);
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

    // Lógica corrigida: Preenche os últimos 12 meses retroativamente a partir de 07/2026
    int mesRef = 7;
    int anoRef = 2026;

    for (int m = 0; m < 12; m++)
    {
        int mes = mesRef - m;
        int ano = anoRef;
        if (mes <= 0)
        {
            mes += 12;
            ano -= 1;
        }

        // Verifica se o mês calculado é anterior à data de contratação
        // Para simplificar, convertemos para um formato numérico comparável (AAAAMM)
        long dataPagNum = (long)ano * 100L + (long)mes;
        long dataContrNum = (long)f.dataContratacao.ano * 100L + (long)f.dataContratacao.mes;

        if (dataPagNum >= dataContrNum)
        {
            f.historico[m].dataPagamento.dia = 1;
            f.historico[m].dataPagamento.mes = mes;
            f.historico[m].dataPagamento.ano = ano;
            f.historico[m].valor = 2500.00;
            f.qtdPagamentos++;
        }
    }

    long novoEndereco = gravarFuncionario(dados, &f);
    inserirChave(arv, chave, novoEndereco);
    printf("Funcionario cadastrado com sucesso!\n");
    free(chave);
}

/**
 * @brief Lida com a opção de busca no menu, procurando e exibindo os dados de um funcionário.
 * 
 * @param arv (ArvoreBPlus*) Ponteiro para a árvore B+.
 * @param dados (FILE*) Ponteiro para o arquivo de dados.
 * @return (void) Não retorna valor.
 */
void opcaoBuscar(ArvoreBPlus *arv, FILE *dados) {
    char nome[100];
    printf("\n--- Buscar Funcionario ---\n");
    printf("Nome: ");
    lerLinha(nome, sizeof(nome));

    ListaResultados lista;
    buscarTodosPorNome(arv, nome, &lista);

    if (lista.qtd == 0)
    {
        printf("Nenhum funcionario encontrado com esse nome.\n");
        return;
    }

    int escolhido = 0;
    if (lista.qtd > 1)
    {
        escolhido = escolherHomonimo(&lista);
        if (escolhido == -1)
        {
            printf("Data de nascimento nao encontrada. Operacao cancelada.\n");
            return;
        }
    }
    Funcionario f = lerFuncionario(dados, lista.enderecos[escolhido]);
    printf("\n--- Ficha do Funcionario ---\n");
    imprimirFichaCompleta(&f);
}

/**
 * @brief Lida com a opção de exclusão no menu, permitindo remover um funcionário do índice.
 * 
 * @param arv (ArvoreBPlus*) Ponteiro para a árvore B+.
 * @param dados (FILE*) Ponteiro para o arquivo de dados.
 * @return (void) Não retorna valor.
 */
void opcaoExcluir(ArvoreBPlus *arv, FILE *dados) {
    char nome[100];
    printf("\n--- Excluir Funcionario ---\n");
    printf("Nome: ");
    lerLinha(nome, sizeof(nome));

    ListaResultados lista;
    buscarTodosPorNome(arv, nome, &lista);

    if (lista.qtd == 0)
    {
        printf("Nenhum funcionario encontrado com esse nome.\n");
        return;
    }

    int escolhido = 0;
    if (lista.qtd > 1)
    {
        escolhido = escolherHomonimo(&lista);
        if (escolhido == -1)
        {
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

    if (resp[0] == 's' || resp[0] == 'S')
    {
        ChaveFuncionario *chave = criarChave(f.nome, f.nascimento.dia, f.nascimento.mes, f.nascimento.ano);
        removerChave(arv, chave);
        free(chave);
        printf("Funcionario removido do indice com sucesso!\n");
    }
    else
    {
        printf("Exclusao cancelada.\n");
    }
}

typedef struct {
    FILE *dados;
    int contador;
} ContextoIntervalo;

/**
 * @brief Função de callback para imprimir informações resumidas de funcionários encontrados em um intervalo.
 * 
 * @param chave (const void*) Chave encontrada durante a travessia.
 * @param endereco (long) Endereço no arquivo de dados.
 * @param contexto (void*) Estrutura de contexto contendo arquivo e contador.
 * @return (void) Não retorna valor.
 */
void imprimirLinhaIntervalo(const void *chave, long endereco, void *contexto) {
    (void)chave;

    ContextoIntervalo *ctx = (ContextoIntervalo *)contexto;
    Funcionario f = lerFuncionario(ctx->dados, endereco);
    ctx->contador++;
    printf("  %d) %s - %02d/%02d/%04d\n", ctx->contador, f.nome,
           f.nascimento.dia, f.nascimento.mes, f.nascimento.ano);
}

/**
 * @brief Lida com a opção de listagem por intervalo no menu, exibindo funcionários num intervalo de nomes.
 * 
 * @param arv (ArvoreBPlus*) Ponteiro para a árvore B+.
 * @param dados (FILE*) Ponteiro para o arquivo de dados.
 * @return (void) Não retorna valor.
 */
void opcaoIntervalo(ArvoreBPlus *arv, FILE *dados) {
    char nomeA[100], nomeB[100];
    printf("\n--- Listagem por Intervalo ---\n");
    printf("Nome A (limite inicial): ");
    lerLinha(nomeA, sizeof(nomeA));
    printf("Nome B (limite final): ");
    lerLinha(nomeB, sizeof(nomeB));

    ChaveFuncionario *chaveA = criarChave(nomeA, 0, 0, 0);
    ChaveFuncionario *chaveB = criarChave(nomeB, 31, 12, 9999);
    ContextoIntervalo ctx = {dados, 0};

    printf("\nFuncionarios entre \"%s\" e \"%s\":\n", nomeA, nomeB);
    buscarIntervalo(arv, chaveA, chaveB, imprimirLinhaIntervalo, &ctx);

    if (ctx.contador == 0)
    {
        printf("  (nenhum funcionario encontrado nesse intervalo)\n");
    }
    free(chaveA);
    free(chaveB);
}

/**
 * @brief Imprime as opções do menu principal na tela.
 * 
 * @return (void) Não retorna valor.
 */
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

/**
 * @brief Ponto de entrada do programa. Inicializa o sistema e exibe o menu principal.
 * 
 * @return (int) Retorna 0 em caso de sucesso, ou 1 em caso de falha.
 */
int main(void) {
    ArvoreBPlus *arvore = criarArvore(ARQ_INDICE, compararChave, GravarChaveFunc, LerChaveFunc, tamanhoChave, liberarChave, imprimirChave);

    FILE *dados = abrirArquivoDados(ARQ_DADOS);
    if (dados == NULL)
    {
        printf("Erro ao abrir o arquivo de dados.\n");
        return 1;
    }

    int opcao;
    do
    {
        exibirMenu();
        opcao = lerInteiro();

        switch (opcao)
        {
        case 1:
            opcaoInserir(arvore, dados);
            break;
        case 2:
            opcaoBuscar(arvore, dados);
            break;
        case 3:
            opcaoExcluir(arvore, dados);
            break;
        case 4:
            opcaoIntervalo(arvore, dados);
            break;
        case 5:
            imprimirEstruturaArvore(arvore);
            break;
        case 6:
            printf("Encerrando o sistema...\n");
            break;
        default:
            printf("Opcao invalida!\n");
            break;
        }
    } while (opcao != 6);

    fclose(dados);
    fecharArvore(arvore);
    return 0;
}