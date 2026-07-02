/*
    ESTRUTURA DA DADOS II - AVALIAÇÃO 2
    PROFESSORA: Dra. Luciana Lee
    ALUNOS: 
        - 2023201331 | Gabriel dos Santos Lima
        - 2023201073 | João Pedro Pardinho Rodrigues
        - 2023200798 | Nicolas Leal Espindula
*/

#include "funcionario.h"

/**
 *  Abre ou cria um arquivo de dados binário para armazenar funcionários.
 * 
 *  parametro: caminho (const char*) O caminho do arquivo a ser aberto ou criado.
 *  retorno: (FILE*) Um ponteiro para o arquivo aberto, ou o arquivo recém-criado.
 */
FILE* abrirArquivoDados(const char *caminho) {
    FILE *arq = fopen(caminho, "rb+");
    if (arq == NULL) {
        arq = fopen(caminho, "wb+");
    }
    return arq;
}

/**
 *  Grava um novo funcionário no final do arquivo de dados.
 * 
 * parametro: arq (FILE*) Ponteiro para o arquivo de dados.
 * parametro: f (const Funcionario*) Ponteiro para o funcionário a ser gravado.
 * retorno: (long) O endereço (offset) onde o funcionário foi gravado no arquivo.
 */
long gravarFuncionario(FILE *arq, const Funcionario *f) {
    long endereco;
    fseek(arq, 0, SEEK_END);
    endereco = ftell(arq);
    fwrite(f, sizeof(Funcionario), 1, arq);
    fflush(arq);
    return endereco;
}

/**
 * Atualiza os dados de um funcionário existente em um endereço específico.
 * 
 * parametro: arq (FILE*) Ponteiro para o arquivo de dados.
 * parametro: endereco (long) Endereço (offset) no arquivo onde o registro será atualizado.
 * parametro: f (const Funcionario*) Ponteiro para os novos dados do funcionário.
 * retorno: (void) Não retorna valor.
 */
void atualizarFuncionario(FILE *arq, long endereco, const Funcionario *f) {
    fseek(arq, endereco, SEEK_SET);
    fwrite(f, sizeof(Funcionario), 1, arq);
    fflush(arq);
}

/**
 * Lê os dados de um funcionário a partir de um endereço específico no arquivo.
 * 
 * parametro: arq (FILE*) Ponteiro para o arquivo de dados.
 * parametro: endereco (long) Endereço (offset) onde o registro se encontra no arquivo.
 * retorno: (Funcionario) Retorna uma estrutura Funcionario contendo os dados lidos.
 */
Funcionario lerFuncionario(FILE *arq, long endereco) {
    Funcionario f;
    fseek(arq, endereco, SEEK_SET);
    fread(&f, sizeof(Funcionario), 1, arq);
    return f;
}

/**
 * Imprime uma versão resumida da ficha do funcionário na saída padrão.
 * 
 * parametro: f (const Funcionario*) Ponteiro para o funcionário que será impresso.
 * retorno: (void) Não retorna valor.
 */
void imprimirFichaResumida(const Funcionario *f) {
    printf("Nome.............: %s\n", f->nome);
    printf("Nascimento.......: %02d/%02d/%04d\n", f->nascimento.dia, f->nascimento.mes, f->nascimento.ano);
    printf("Mae..............: %s\n", f->nomeMae);
    printf("Pai..............: %s\n", f->nomePai);
    printf("Endereco.........: %s\n", f->endereco);
    printf("Telefone.........: %s\n", f->telefone);
    printf("Contratacao......: %02d/%02d/%04d\n", f->dataContratacao.dia, f->dataContratacao.mes, f->dataContratacao.ano);
    printf("Status...........: %s\n", f->ativo ? "Ativo" : "Inativo");
    if (!f->ativo) {
        printf("Desligamento.....: %02d/%02d/%04d\n", f->dataDesligamento.dia, f->dataDesligamento.mes, f->dataDesligamento.ano);
    }
}

/**
 * Imprime a ficha completa do funcionário, incluindo o histórico de pagamentos.
 * 
 * parametro: f (const Funcionario*) Ponteiro para o funcionário que será impresso.
 * retorno: (void) Não retorna valor.
 */
void imprimirFichaCompleta(const Funcionario *f) {
    imprimirFichaResumida(f);

    printf("Historico de pagamentos (ultimos 12 meses):\n");
    int exibidos = 0;
    
    // Data de referência atual: 01/07/2026
    int anoAtual = 2026;
    int mesAtual = 7;

    for (int i = 0; i < f->qtdPagamentos; i++) {
        int pagAno = f->historico[i].dataPagamento.ano;
        int pagMes = f->historico[i].dataPagamento.mes;

        // Calcula a diferença em meses de forma linear
        int diffMeses = ((anoAtual - pagAno) * 12) + (mesAtual - pagMes);

        // Se a diferença estiver entre 0 e 11, o pagamento é recente
        if (diffMeses >= 0 && diffMeses < 12) {
            printf("   %02d/%02d/%04d - R$ %.2f\n",
                   f->historico[i].dataPagamento.dia,
                   pagMes,
                   pagAno,
                   f->historico[i].valor);
            exibidos++;
        }
    }
    
    if (exibidos == 0) {
        printf("   (nenhum pagamento registrado nos ultimos 12 meses)\n");
    }
}