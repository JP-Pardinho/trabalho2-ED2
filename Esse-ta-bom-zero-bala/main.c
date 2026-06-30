#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Bplus1.h"
#include "funcionarios.h"

#define ARQUIVO_DADOS "registos.bin"
#define ARQUIVO_INDICE "indice_rh.bin"

// =======================================================
// Variáveis Globais
// =======================================================
FILE *arquivo_dados;

// Variáveis para auxiliar na recolha de homónimos durante a busca
ChaveFuncionario chaves_encontradas[100];
int indexes_encontrados[100];
int num_encontrados = 0;

// =======================================================
// Funções Utilitárias
// =======================================================
void limpar_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void ler_string(char *destino, int tamanho) {
    fgets(destino, tamanho, stdin);
    destino[strcspn(destino, "\n")] = 0; // Remove o \n final
}

// =======================================================
// Manipulação do Ficheiro de Dados (registos.bin)
// =======================================================
int salvar_funcionario(Funcionario *f) {
    fseek(arquivo_dados, 0, SEEK_END);
    int index = ftell(arquivo_dados) / sizeof(Funcionario);
    fwrite(f, sizeof(Funcionario), 1, arquivo_dados);
    fflush(arquivo_dados);
    return index;
}

void atualizar_funcionario(int index, Funcionario *f) {
    fseek(arquivo_dados, index * sizeof(Funcionario), SEEK_SET);
    fwrite(f, sizeof(Funcionario), 1, arquivo_dados);
    fflush(arquivo_dados);
}

Funcionario ler_funcionario(int index) {
    Funcionario f;
    fseek(arquivo_dados, index * sizeof(Funcionario), SEEK_SET);
    fread(&f, sizeof(Funcionario), 1, arquivo_dados);
    return f;
}

// =======================================================
// Callbacks para a Árvore B+
// =======================================================
void callback_homonimos(void *chave, int index_registro) {
    chaves_encontradas[num_encontrados] = *(ChaveFuncionario*)chave;
    indexes_encontrados[num_encontrados] = index_registro;
    num_encontrados++;
}

void callback_imprimir_intervalo(void *chave, int index_registro) {
    (void)chave;
    Funcionario f = ler_funcionario(index_registro);
    printf("- %s (Nascido a: %02d/%02d/%04d) | Telefone: %s\n", 
           f.nome, f.nascimento.dia, f.nascimento.mes, f.nascimento.ano, f.telefone);
}

// =======================================================
// Lógica de Negócio: Resolução de Homónimos
// =======================================================
int procurar_por_nome(ArvoreBPlus *arvore, char *nome_buscado) {
    ChaveFuncionario inicio, fim;
    
    // Configura os limites para capturar TODAS as datas possíveis deste nome
    strcpy(inicio.nome, nome_buscado);
    inicio.nascimento.dia = 0; inicio.nascimento.mes = 0; inicio.nascimento.ano = 0;
    
    strcpy(fim.nome, nome_buscado);
    fim.nascimento.dia = 31; fim.nascimento.mes = 12; fim.nascimento.ano = 9999;
    
    num_encontrados = 0;
    buscar_intervalo(arvore, &inicio, &fim, callback_homonimos);
    
    if (num_encontrados == 0) {
        return -1; // Não encontrado
    }
    
    if (num_encontrados == 1) {
        return indexes_encontrados[0]; // Nome único
    }
    
    // Múltiplos homónimos encontrados! Aplica critério de desempate
    printf("\n[!] Foram encontrados multiplos registos com o nome '%s':\n", nome_buscado);
    for (int i = 0; i < num_encontrados; i++) {
        printf("  %d. Data de Nascimento: %02d/%02d/%04d\n", i + 1, 
               chaves_encontradas[i].nascimento.dia, 
               chaves_encontradas[i].nascimento.mes, 
               chaves_encontradas[i].nascimento.ano);
    }
    
    printf("\nIntroduza a Data de Nascimento para desempate (DD/MM/AAAA): ");
    Data d;
    scanf("%d/%d/%d", &d.dia, &d.mes, &d.ano);
    limpar_buffer();
    
    for (int i = 0; i < num_encontrados; i++) {
        if (chaves_encontradas[i].nascimento.dia == d.dia &&
            chaves_encontradas[i].nascimento.mes == d.mes &&
            chaves_encontradas[i].nascimento.ano == d.ano) {
            return indexes_encontrados[i];
        }
    }
    
    printf("Nenhum registo corresponde a essa data de nascimento.\n");
    return -1;
}

// =======================================================
// Função Principal e Menu Interativo
// =======================================================
int main() {
    // 1. Inicialização dos ficheiros
    arquivo_dados = fopen(ARQUIVO_DADOS, "rb+");
    if (arquivo_dados == NULL) {
        arquivo_dados = fopen(ARQUIVO_DADOS, "wb+");
    }

    ArvoreBPlus *arvore = criar_arvore(ARQUIVO_INDICE, 4, sizeof(ChaveFuncionario), comparar_chaves_funcionario);

    int opcao;
    do {
        printf("\n===========================================\n");
        printf("     SISTEMA DE GESTAO DE RH (B+ TREE)     \n");
        printf("===========================================\n");
        printf("1. Inserir Funcionario\n");
        printf("2. Buscar Funcionario\n");
        printf("3. Excluir Funcionario\n");
        printf("4. Listar por Intervalo\n");
        printf("5. Exibir Estrutura do Indice\n");
        printf("6. Sair\n");
        printf("===========================================\n");
        printf("Escolha uma opcao: ");
        scanf("%d", &opcao);
        limpar_buffer();

        switch (opcao) {
            case 1: { // Inserir Funcionario
                ChaveFuncionario chave_nova;
                printf("Nome do Funcionario: ");
                ler_string(chave_nova.nome, 100);
                printf("Data de Nascimento (DD/MM/AAAA): ");
                scanf("%d/%d/%d", &chave_nova.nascimento.dia, &chave_nova.nascimento.mes, &chave_nova.nascimento.ano);
                limpar_buffer();

                int index_existente;
                if (buscar_registro(arvore, &chave_nova, &index_existente)) {
                    printf("\n[!] O funcionario ja existe no sistema!\n");
                    Funcionario existente = ler_funcionario(index_existente);
                    printf("Contacto atual: %s | Morada: %s\n", existente.telefone, existente.endereco);
                    
                    char resp;
                    printf("Deseja atualizar os dados? (S/N): ");
                    scanf("%c", &resp);
                    limpar_buffer();
                    
                    if (resp == 'S' || resp == 's') {
                        printf("Novo Telefone: ");
                        ler_string(existente.telefone, 20);
                        printf("Nova Morada: ");
                        ler_string(existente.endereco, 200);
                        atualizar_funcionario(index_existente, &existente);
                        printf("Dados atualizados com sucesso!\n");
                    }
                } else {
                    Funcionario novo;
                    strcpy(novo.nome, chave_nova.nome);
                    novo.nascimento = chave_nova.nascimento;
                    
                    printf("Nome da Mae: "); ler_string(novo.nomeMae, 100);
                    printf("Nome do Pai: "); ler_string(novo.nomePai, 100);
                    printf("Morada: "); ler_string(novo.endereco, 200);
                    printf("Telefone: "); ler_string(novo.telefone, 20);
                    
                    printf("Data de Contratacao (DD/MM/AAAA): ");
                    scanf("%d/%d/%d", &novo.contratacao.dia, &novo.contratacao.mes, &novo.contratacao.ano);
                    novo.ativo = 1;
                    memset(novo.historicoPagamentos, 0, sizeof(novo.historicoPagamentos));
                    limpar_buffer();

                    int novo_index = salvar_funcionario(&novo);
                    inserir_registro(arvore, &chave_nova, novo_index);
                    printf("\n=> Funcionario indexado e guardado com sucesso!\n");
                }
                break;
            }
            case 2: { // Buscar
                char nome[100];
                printf("Introduza o Nome a procurar: ");
                ler_string(nome, 100);
                
                int idx = procurar_por_nome(arvore, nome);
                if (idx != -1) {
                    Funcionario f = ler_funcionario(idx);
                    printf("\n--- FICHA DO FUNCIONARIO ---\n");
                    printf("Nome: %s\n", f.nome);
                    printf("Nascimento: %02d/%02d/%04d\n", f.nascimento.dia, f.nascimento.mes, f.nascimento.ano);
                    printf("Filiacao: %s e %s\n", f.nomeMae, f.nomePai);
                    printf("Moradia: %s\n", f.endereco);
                    printf("Telefone: %s\n", f.telefone);
                    printf("Estado: %s\n", f.ativo ? "Ativo" : "Inativo");
                    printf("Historico de Pagamentos (Ultimos 12 meses): [");
                    for(int i=0; i<12; i++) printf("%.2f ", f.historicoPagamentos[i]);
                    printf("]\n----------------------------\n");
                } else {
                    printf("\nFuncionario nao encontrado.\n");
                }
                break;
            }
            case 3: { // Excluir
                char nome[100];
                printf("Introduza o Nome a excluir: ");
                ler_string(nome, 100);
                
                int idx = procurar_por_nome(arvore, nome);
                if (idx != -1) {
                    Funcionario f = ler_funcionario(idx);
                    printf("\nConfirmacao de Exclusao (Dados Parciais):\n");
                    printf("Nome: %s | Nascimento: %02d/%02d/%04d | Telefone: %s\n", 
                           f.nome, f.nascimento.dia, f.nascimento.mes, f.nascimento.ano, f.telefone);
                           
                    char resp;
                    printf("Tem a certeza que deseja remover este registo? (S/N): ");
                    scanf("%c", &resp);
                    limpar_buffer();
                    
                    if (resp == 'S' || resp == 's') {
                        ChaveFuncionario chave_remocao;
                        strcpy(chave_remocao.nome, f.nome);
                        chave_remocao.nascimento = f.nascimento;
                        
                        if(remover_registro(arvore, &chave_remocao)) {
                            printf("=> Registo removido com sucesso da Arvore B+!\n");
                            // Opcional: Marcar no ficheiro de dados como inativo/eliminado
                            f.ativo = 0;
                            atualizar_funcionario(idx, &f);
                        } else {
                            printf("Erro critico: Registo nao encontrado na arvore.\n");
                        }
                    } else {
                        printf("Operacao cancelada.\n");
                    }
                } else {
                    printf("\nFuncionario nao encontrado.\n");
                }
                break;
            }
            case 4: { // Listar por Intervalo
                ChaveFuncionario inicio = {"", {31,12,9999}}; // Começa APÓS a primeira string
                ChaveFuncionario fim = {"", {0,0,0}};         // Termina ANTES da segunda string
                
                printf("Introduza o Nome Inicial (Nome A): ");
                ler_string(inicio.nome, 100);
                printf("Introduza o Nome Final (Nome B): ");
                ler_string(fim.nome, 100);
                
                printf("\n--- RESULTADOS NO INTERVALO ABERTO (%s, %s) ---\n", inicio.nome, fim.nome);
                buscar_intervalo(arvore, &inicio, &fim, callback_imprimir_intervalo);
                printf("--------------------------------------------------\n");
                break;
            }
            case 5: { // Exibir Índice
                printf("\n--- ESTRUTURA FISICA DA ARVORE B+ ---\n");
                imprimir_arvore(arvore, imprimir_chave_funcionario);
                break;
            }
            case 6: { // Sair
                printf("A guardar os ficheiros e a encerrar o sistema. Ate a proxima!\n");
                fechar_arvore(arvore);
                if (arquivo_dados != NULL) fclose(arquivo_dados);
                break;
            }
            default:
                printf("Opcao invalida. Tente novamente.\n");
        }
    } while (opcao != 6);

    return 0;
}