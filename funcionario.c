#include "funcionario.h"

FILE* abrirArquivoDados(const char *caminho) {
    FILE *arq = fopen(caminho, "rb+");
    if (arq == NULL) {
        arq = fopen(caminho, "wb+");
    }
    return arq;
}

long gravarFuncionario(FILE *arq, const Funcionario *f) {
    long endereco;
    fseek(arq, 0, SEEK_END);
    endereco = ftell(arq);
    fwrite(f, sizeof(Funcionario), 1, arq);
    fflush(arq);
    return endereco;
}

void atualizarFuncionario(FILE *arq, long endereco, const Funcionario *f) {
    fseek(arq, endereco, SEEK_SET);
    fwrite(f, sizeof(Funcionario), 1, arq);
    fflush(arq);
}

Funcionario lerFuncionario(FILE *arq, long endereco) {
    Funcionario f;
    fseek(arq, endereco, SEEK_SET);
    fread(&f, sizeof(Funcionario), 1, arq);
    return f;
}

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