/* ============================================================================
 * funcionario.c
 *
 * Implementacao das funcoes de manipulacao do Funcionario, da chave composta
 * (Nome + Data de Nascimento) e do arquivo de dados de funcionarios.
 * ==========================================================================*/

#include "funcionario.h"
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ---------------------------------------------------------------------------
 * Utilitarios de Data
 * ------------------------------------------------------------------------- */

static int dias_no_mes(int mes, int ano) {
    int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (mes == 2) {
        int bissexto = (ano % 4 == 0 && (ano % 100 != 0 || ano % 400 == 0));
        return bissexto ? 29 : 28;
    }
    if (mes < 1 || mes > 12) return 31;
    return dm[mes - 1];
}

int data_valida(Data d) {
    if (d.ano < 1900 || d.ano > 2100) return 0;
    if (d.mes < 1 || d.mes > 12) return 0;
    if (d.dia < 1 || d.dia > dias_no_mes(d.mes, d.ano)) return 0;
    return 1;
}

int data_comparar(Data a, Data b) {
    if (a.ano != b.ano) return a.ano - b.ano;
    if (a.mes != b.mes) return a.mes - b.mes;
    return a.dia - b.dia;
}

void data_imprimir(Data d) {
    printf("%02d/%02d/%04d", d.dia, d.mes, d.ano);
}

void data_hoje(Data *d) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    d->dia = lt->tm_mday;
    d->mes = lt->tm_mon + 1;
    d->ano = lt->tm_year + 1900;
}

void data_ler_teclado(const char *rotulo, Data *d) {
    char linha[64];
    while (1) {
        printf("%s (DD/MM/AAAA): ", rotulo);
        if (!fgets(linha, sizeof(linha), stdin)) continue;
        int dia, mes, ano;
        if (sscanf(linha, "%d/%d/%d", &dia, &mes, &ano) == 3) {
            Data tmp = { dia, mes, ano };
            if (data_valida(tmp)) {
                *d = tmp;
                return;
            }
        }
        printf("Data invalida. Tente novamente (ex: 15/03/1990).\n");
    }
}

/* ---------------------------------------------------------------------------
 * Chave composta: criacao e comparacao
 * ---------------------------------------------------------------------------
 * Criterio principal: Nome (ordem alfabetica, case-insensitive)
 * Criterio de desempate: Data de nascimento
 * ------------------------------------------------------------------------- */

ChaveComposta *chave_criar(const char *nome, Data nascimento) {
    ChaveComposta *c = (ChaveComposta *)malloc(sizeof(ChaveComposta));
    strncpy(c->nome, nome, TAM_NOME - 1);
    c->nome[TAM_NOME - 1] = '\0';
    c->data_nascimento = nascimento;
    return c;
}

int chave_comparar_nome(const char *nomeA, const char *nomeB) {
    /* comparacao alfabetica case-insensitive */
    size_t i = 0;
    while (nomeA[i] && nomeB[i]) {
        unsigned char ca = (unsigned char)tolower((unsigned char)nomeA[i]);
        unsigned char cb = (unsigned char)tolower((unsigned char)nomeB[i]);
        if (ca != cb) return (int)ca - (int)cb;
        i++;
    }
    return (unsigned char)tolower((unsigned char)nomeA[i]) - (unsigned char)tolower((unsigned char)nomeB[i]);
}

int chave_comparar(const void *a, const void *b) {
    const ChaveComposta *ca = (const ChaveComposta *)a;
    const ChaveComposta *cb = (const ChaveComposta *)b;
    int c = chave_comparar_nome(ca->nome, cb->nome);
    if (c != 0) return c;
    return data_comparar(ca->data_nascimento, cb->data_nascimento);
}

/* ---------------------------------------------------------------------------
 * Serializacao da chave composta (para a Arvore B+ generica)
 * Layout no buffer: [nome (TAM_NOME bytes)] [dia][mes][ano] (3 ints)
 * ------------------------------------------------------------------------- */

void chave_escrever(const void *chave, unsigned char *buf) {
    const ChaveComposta *c = (const ChaveComposta *)chave;
    memset(buf, 0, TAM_NOME);
    memcpy(buf, c->nome, TAM_NOME);
    unsigned char *p = buf + TAM_NOME;
    memcpy(p, &c->data_nascimento.dia, sizeof(int)); p += sizeof(int);
    memcpy(p, &c->data_nascimento.mes, sizeof(int)); p += sizeof(int);
    memcpy(p, &c->data_nascimento.ano, sizeof(int));
}

void *chave_ler(const unsigned char *buf) {
    ChaveComposta *c = (ChaveComposta *)malloc(sizeof(ChaveComposta));
    memcpy(c->nome, buf, TAM_NOME);
    const unsigned char *p = buf + TAM_NOME;
    memcpy(&c->data_nascimento.dia, p, sizeof(int)); p += sizeof(int);
    memcpy(&c->data_nascimento.mes, p, sizeof(int)); p += sizeof(int);
    memcpy(&c->data_nascimento.ano, p, sizeof(int));
    return c;
}

int chave_tamanho(void) {
    return TAM_NOME + 3 * (int)sizeof(int);
}

void chave_liberar(void *chave) {
    free(chave);
}

void chave_imprimir(const void *chave) {
    const ChaveComposta *c = (const ChaveComposta *)chave;
    /* imprime so o PRIMEIRO nome + data de nascimento, conforme exigido no
     * requisito de impressao hierarquica da arvore */
    char primeiro_nome[TAM_NOME];
    strncpy(primeiro_nome, c->nome, TAM_NOME - 1);
    primeiro_nome[TAM_NOME - 1] = '\0';
    char *espaco = strchr(primeiro_nome, ' ');
    if (espaco) *espaco = '\0';
    printf("%s %02d/%02d/%04d", primeiro_nome,
           c->data_nascimento.dia, c->data_nascimento.mes, c->data_nascimento.ano);
}

/* ---------------------------------------------------------------------------
 * Arquivo de dados de funcionarios
 * ---------------------------------------------------------------------------
 * Cabecalho proprio (topo + lista_livres), seguido de registros de tamanho
 * FIXO (sizeof(Funcionario)), permitindo reaproveitamento de blocos livres
 * exatamente como no arquivo de indice da arvore B+.
 * ------------------------------------------------------------------------- */

typedef struct {
    long topo;
    long lista_livres;
} CabecalhoFuncArquivo;

#define OFFSET_CAB_FUNC 0L
#define OFFSET_PRIMEIRO_REG ((long)sizeof(CabecalhoFuncArquivo))

ArquivoFuncionarios *func_arquivo_abrir(const char *caminho) {
    ArquivoFuncionarios *af = (ArquivoFuncionarios *)malloc(sizeof(ArquivoFuncionarios));
    strncpy(af->caminho, caminho, sizeof(af->caminho) - 1);
    af->caminho[sizeof(af->caminho) - 1] = '\0';

    FILE *teste = fopen(caminho, "rb");
    if (!teste) {
        af->arquivo = fopen(caminho, "wb+");
        af->topo = OFFSET_PRIMEIRO_REG;
        af->lista_livres = BPLUS_NULL;
        CabecalhoFuncArquivo cab = { af->topo, af->lista_livres };
        fseek(af->arquivo, OFFSET_CAB_FUNC, SEEK_SET);
        fwrite(&cab, sizeof(cab), 1, af->arquivo);
        fflush(af->arquivo);
        return af;
    }
    fclose(teste);

    af->arquivo = fopen(caminho, "rb+");
    CabecalhoFuncArquivo cab;
    fseek(af->arquivo, OFFSET_CAB_FUNC, SEEK_SET);
    fread(&cab, sizeof(cab), 1, af->arquivo);
    af->topo = cab.topo;
    af->lista_livres = cab.lista_livres;
    return af;
}

void func_arquivo_fechar(ArquivoFuncionarios *af) {
    if (!af) return;
    CabecalhoFuncArquivo cab = { af->topo, af->lista_livres };
    fseek(af->arquivo, OFFSET_CAB_FUNC, SEEK_SET);
    fwrite(&cab, sizeof(cab), 1, af->arquivo);
    fflush(af->arquivo);
    fclose(af->arquivo);
    free(af);
}

static long func_alocar_bloco(ArquivoFuncionarios *af) {
    if (af->lista_livres != BPLUS_NULL) {
        long offset = af->lista_livres;
        long proximo;
        fseek(af->arquivo, offset, SEEK_SET);
        fread(&proximo, sizeof(long), 1, af->arquivo);
        af->lista_livres = proximo;
        return offset;
    }
    long offset = af->topo;
    af->topo += sizeof(Funcionario);
    return offset;
}

void func_arquivo_remover(ArquivoFuncionarios *af, long offset) {
    long proximo_livre = af->lista_livres;
    fseek(af->arquivo, offset, SEEK_SET);
    fwrite(&proximo_livre, sizeof(long), 1, af->arquivo);
    /* preenche o resto do bloco com zeros para nao deixar lixo/dados antigos */
    static const char zeros[sizeof(Funcionario)] = {0};
    fwrite(zeros + sizeof(long), sizeof(Funcionario) - sizeof(long), 1, af->arquivo);
    fflush(af->arquivo);
    af->lista_livres = offset;
}

long func_arquivo_inserir(ArquivoFuncionarios *af, const Funcionario *f) {
    long offset = func_alocar_bloco(af);
    Funcionario copia = *f;
    copia.offset_proprio = offset;
    fseek(af->arquivo, offset, SEEK_SET);
    fwrite(&copia, sizeof(Funcionario), 1, af->arquivo);
    fflush(af->arquivo);
    return offset;
}

void func_arquivo_atualizar(ArquivoFuncionarios *af, long offset, const Funcionario *f) {
    Funcionario copia = *f;
    copia.offset_proprio = offset;
    fseek(af->arquivo, offset, SEEK_SET);
    fwrite(&copia, sizeof(Funcionario), 1, af->arquivo);
    fflush(af->arquivo);
}

void func_arquivo_ler(ArquivoFuncionarios *af, long offset, Funcionario *f) {
    fseek(af->arquivo, offset, SEEK_SET);
    fread(f, sizeof(Funcionario), 1, af->arquivo);
}
