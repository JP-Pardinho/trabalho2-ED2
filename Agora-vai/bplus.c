#include "bplus.h"
#include <string.h>
#include <assert.h>

typedef struct {
    long self;                                   /* offset deste no no arquivo */
    int eh_folha;
    int n_chaves;
    unsigned char chaves[BPLUS_MAX_CHAVES + 1][BPLUS_TAM_MAX_CHAVE];

    long filhos[BPLUS_ORDEM + 1];                 /* usado se interno (n_chaves+1 validos) */

    long dados[BPLUS_MAX_CHAVES + 1];             /* usado se folha */
    long proxima_folha;                           /* usado se folha */
} NoMem;

/* Cabecalho do arquivo de indice */
typedef struct {
    long raiz;
    long topo;
    long lista_livres;
} Cabecalho;

#define OFFSET_CABECALHO 0L
#define OFFSET_PRIMEIRO_NO ((long)sizeof(Cabecalho))

/* ---------------------------------------------------------------------------
 * Serializacao / Deserializacao de nos (tamanho fixo em disco)
 * ------------------------------------------------------------------------- */

static long tamanho_no_disco(void) {
    /* precisa ser fixo e grande o suficiente para o maior layout possivel */
    long tam = sizeof(int) * 2; /* eh_folha, n_chaves */
    tam += (long)BPLUS_MAX_CHAVES * BPLUS_TAM_MAX_CHAVE;
    long tam_interno = tam + (long)sizeof(long) * BPLUS_ORDEM;
    long tam_folha   = tam + (long)sizeof(long) * BPLUS_MAX_CHAVES + sizeof(long);
    return (tam_interno > tam_folha) ? tam_interno : tam_folha;
}

/* Escreve um NoMem inteiro (layout fixo) em um buffer de bytes */
static void serializar_no(const NoMem *no, unsigned char *buf, int tam_buf) {
    memset(buf, 0, tam_buf);
    unsigned char *p = buf;
    memcpy(p, &no->eh_folha, sizeof(int)); p += sizeof(int);
    memcpy(p, &no->n_chaves, sizeof(int)); p += sizeof(int);
    memcpy(p, no->chaves, (size_t)BPLUS_MAX_CHAVES * BPLUS_TAM_MAX_CHAVE);
    p += (size_t)BPLUS_MAX_CHAVES * BPLUS_TAM_MAX_CHAVE;

    if (no->eh_folha) {
        memcpy(p, no->dados, sizeof(long) * BPLUS_MAX_CHAVES);
        p += sizeof(long) * BPLUS_MAX_CHAVES;
        memcpy(p, &no->proxima_folha, sizeof(long));
    } else {
        memcpy(p, no->filhos, sizeof(long) * BPLUS_ORDEM);
    }
}

static void deserializar_no(NoMem *no, const unsigned char *buf) {
    const unsigned char *p = buf;
    memcpy(&no->eh_folha, p, sizeof(int)); p += sizeof(int);
    memcpy(&no->n_chaves, p, sizeof(int)); p += sizeof(int);
    memcpy(no->chaves, p, (size_t)BPLUS_MAX_CHAVES * BPLUS_TAM_MAX_CHAVE);
    p += (size_t)BPLUS_MAX_CHAVES * BPLUS_TAM_MAX_CHAVE;

    if (no->eh_folha) {
        memcpy(no->dados, p, sizeof(long) * BPLUS_MAX_CHAVES);
        p += sizeof(long) * BPLUS_MAX_CHAVES;
        memcpy(&no->proxima_folha, p, sizeof(long));
        /* zera area de filhos para evitar lixo */
        memset(no->filhos, 0, sizeof(no->filhos));
    } else {
        memcpy(no->filhos, p, sizeof(long) * BPLUS_ORDEM);
        memset(no->dados, 0, sizeof(no->dados));
        no->proxima_folha = BPLUS_NULL;
    }
}

/* ---------------------------------------------------------------------------
 * E/S FISICA: leitura e escrita de nos e do cabecalho (fseek/fread/fwrite)
 * ------------------------------------------------------------------------- */

static void ler_cabecalho(BPlusTree *arv, Cabecalho *cab) {
    fseek(arv->arquivo, OFFSET_CABECALHO, SEEK_SET);
    fread(cab, sizeof(Cabecalho), 1, arv->arquivo);
}

static void gravar_cabecalho(BPlusTree *arv) {
    Cabecalho cab;
    cab.raiz = arv->raiz;
    cab.topo = arv->topo;
    cab.lista_livres = arv->lista_livres;
    fseek(arv->arquivo, OFFSET_CABECALHO, SEEK_SET);
    fwrite(&cab, sizeof(Cabecalho), 1, arv->arquivo);
    fflush(arv->arquivo);
}

static void ler_no(BPlusTree *arv, long offset, NoMem *no) {
    unsigned char *buf = (unsigned char *)malloc(arv->tam_no);
    fseek(arv->arquivo, offset, SEEK_SET);
    fread(buf, arv->tam_no, 1, arv->arquivo);
    deserializar_no(no, buf);
    no->self = offset;
    free(buf);
}

static void gravar_no(BPlusTree *arv, const NoMem *no) {
    unsigned char *buf = (unsigned char *)malloc(arv->tam_no);
    serializar_no(no, buf, arv->tam_no);
    fseek(arv->arquivo, no->self, SEEK_SET);
    fwrite(buf, arv->tam_no, 1, arv->arquivo);
    fflush(arv->arquivo);
    free(buf);
}

/* ---------------------------------------------------------------------------
 * GERENCIAMENTO DE ESPACO EM DISCO
 * Reaproveita blocos livres (lista encadeada via campo 'filhos[0]' do no
 * livre, reinterpretado como "proximo livre") ou cresce linearmente o
 * arquivo (topo) quando nao ha blocos livres disponiveis.
 * ------------------------------------------------------------------------- */

static long alocar_bloco(BPlusTree *arv) {
    if (arv->lista_livres != BPLUS_NULL) {
        /* reaproveita bloco livre: le o "proximo" gravado nele */
        long offset = arv->lista_livres;
        unsigned char *buf = (unsigned char *)malloc(arv->tam_no);
        fseek(arv->arquivo, offset, SEEK_SET);
        fread(buf, arv->tam_no, 1, arv->arquivo);
        long proximo;
        memcpy(&proximo, buf, sizeof(long)); /* proximo livre foi gravado no inicio */
        free(buf);
        arv->lista_livres = proximo;
        return offset;
    }
    /* crescimento linear organizado */
    long offset = arv->topo;
    arv->topo += arv->tam_no;
    return offset;
}

static void liberar_bloco(BPlusTree *arv, long offset) {
    /* grava, no inicio do bloco, o offset do proximo livre (encadeamento) */
    unsigned char *buf = (unsigned char *)calloc(1, arv->tam_no);
    memcpy(buf, &arv->lista_livres, sizeof(long));
    fseek(arv->arquivo, offset, SEEK_SET);
    fwrite(buf, arv->tam_no, 1, arv->arquivo);
    fflush(arv->arquivo);
    free(buf);
    arv->lista_livres = offset;
}

/* ---------------------------------------------------------------------------
 * Utilitarios de chave
 * ------------------------------------------------------------------------- */

static void chave_para_buf(BPlusTree *arv, const void *chave, unsigned char *buf) {
    memset(buf, 0, BPLUS_TAM_MAX_CHAVE);
    arv->key_write(chave, buf);
}

static void *buf_para_chave(BPlusTree *arv, const unsigned char *buf) {
    return arv->key_read(buf);
}

/* ---------------------------------------------------------------------------
 * Criacao / Abertura / Fechamento
 * ------------------------------------------------------------------------- */

static BPlusTree *alocar_estrutura(const char *caminho,
                                    bplus_cmp_fn cmp,
                                    bplus_key_write_fn kw,
                                    bplus_key_read_fn kr,
                                    bplus_key_size_fn ks,
                                    bplus_key_free_fn kf,
                                    bplus_key_print_fn kp) {
    BPlusTree *arv = (BPlusTree *)malloc(sizeof(BPlusTree));
    strncpy(arv->caminho, caminho, sizeof(arv->caminho) - 1);
    arv->caminho[sizeof(arv->caminho) - 1] = '\0';
    arv->cmp = cmp;
    arv->key_write = kw;
    arv->key_read = kr;
    arv->key_size = ks;
    arv->key_free = kf;
    arv->key_print = kp;
    arv->tam_chave = ks();
    if (arv->tam_chave > BPLUS_TAM_MAX_CHAVE) {
        fprintf(stderr, "ERRO FATAL: tamanho de chave (%d) excede BPLUS_TAM_MAX_CHAVE (%d)\n",
                arv->tam_chave, BPLUS_TAM_MAX_CHAVE);
        exit(1);
    }
    arv->tam_no = (int)tamanho_no_disco();
    return arv;
}

BPlusTree *bplus_criar(const char *caminho,
                        bplus_cmp_fn cmp, bplus_key_write_fn kw, bplus_key_read_fn kr,
                        bplus_key_size_fn ks, bplus_key_free_fn kf, bplus_key_print_fn kp) {
    BPlusTree *arv = alocar_estrutura(caminho, cmp, kw, kr, ks, kf, kp);

    arv->arquivo = fopen(caminho, "wb+");
    if (!arv->arquivo) {
        fprintf(stderr, "ERRO: nao foi possivel criar arquivo de indice '%s'\n", caminho);
        free(arv);
        return NULL;
    }
    arv->raiz = BPLUS_NULL;
    arv->topo = OFFSET_PRIMEIRO_NO;
    arv->lista_livres = BPLUS_NULL;
    gravar_cabecalho(arv);
    return arv;
}

BPlusTree *bplus_abrir(const char *caminho,
                        bplus_cmp_fn cmp, bplus_key_write_fn kw, bplus_key_read_fn kr,
                        bplus_key_size_fn ks, bplus_key_free_fn kf, bplus_key_print_fn kp) {
    FILE *teste = fopen(caminho, "rb");
    if (!teste) {
        /* nao existe: cria novo */
        return bplus_criar(caminho, cmp, kw, kr, ks, kf, kp);
    }
    fclose(teste);

    BPlusTree *arv = alocar_estrutura(caminho, cmp, kw, kr, ks, kf, kp);
    arv->arquivo = fopen(caminho, "rb+");
    if (!arv->arquivo) {
        fprintf(stderr, "ERRO: nao foi possivel abrir arquivo de indice '%s'\n", caminho);
        free(arv);
        return NULL;
    }
    Cabecalho cab;
    ler_cabecalho(arv, &cab);
    arv->raiz = cab.raiz;
    arv->topo = cab.topo;
    arv->lista_livres = cab.lista_livres;
    return arv;
}

void bplus_fechar(BPlusTree *arv) {
    if (!arv) return;
    gravar_cabecalho(arv);
    fclose(arv->arquivo);
    free(arv);
}

/* ---------------------------------------------------------------------------
 * BUSCA
 * ------------------------------------------------------------------------- */

/* Desce da raiz até a folha que deveria conter 'chave'. Retorna o offset da
 * folha. Preenche opcionalmente o caminho de ancestrais em pilhas paralelas
 * (usado pela insercao/remocao para propagar splits/merges). */
#define MAX_ALTURA 64

static long descer_para_folha(BPlusTree *arv, const void *chave,
                               long *pilha_offsets, int *pilha_idx_filho, int *altura_saida) {
    long atual = arv->raiz;
    int altura = 0;
    while (1) {
        NoMem no;
        ler_no(arv, atual, &no);
        if (no.eh_folha) {
            if (altura_saida) *altura_saida = altura;
            return atual;
        }
        /* decide qual filho seguir */
        int i = 0;
        while (i < no.n_chaves) {
            void *k = buf_para_chave(arv, no.chaves[i]);
            int c = arv->cmp(chave, k);
            arv->key_free(k);
            if (c < 0) break;
            i++;
        }
        if (pilha_offsets) pilha_offsets[altura] = atual;
        if (pilha_idx_filho) pilha_idx_filho[altura] = i;
        altura++;
        atual = no.filhos[i];
    }
}

int bplus_buscar(BPlusTree *arv, const void *chave, long *offset_saida) {
    if (arv->raiz == BPLUS_NULL) return 0;
    long folha_off = descer_para_folha(arv, chave, NULL, NULL, NULL);
    NoMem folha;
    ler_no(arv, folha_off, &folha);
    for (int i = 0; i < folha.n_chaves; i++) {
        void *k = buf_para_chave(arv, folha.chaves[i]);
        int c = arv->cmp(chave, k);
        arv->key_free(k);
        if (c == 0) {
            if (offset_saida) *offset_saida = folha.dados[i];
            return 1;
        }
    }
    return 0;
}

int bplus_atualizar_offset(BPlusTree *arv, const void *chave, long novo_offset) {
    if (arv->raiz == BPLUS_NULL) return 0;
    long folha_off = descer_para_folha(arv, chave, NULL, NULL, NULL);
    NoMem folha;
    ler_no(arv, folha_off, &folha);
    for (int i = 0; i < folha.n_chaves; i++) {
        void *k = buf_para_chave(arv, folha.chaves[i]);
        int c = arv->cmp(chave, k);
        arv->key_free(k);
        if (c == 0) {
            folha.dados[i] = novo_offset;
            gravar_no(arv, &folha);
            return 1;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * INSERCAO
 * ------------------------------------------------------------------------- */

/* Insere (chave,offset) num no folha ja lido em memoria (assume espaco livre
 * ou sera splitado por quem chamou). Mantém ordenacao. */
static void inserir_em_folha_ordenado(BPlusTree *arv, NoMem *folha, const void *chave, long offset_dado) {
    int i = folha->n_chaves - 1;
    unsigned char buf_chave[BPLUS_TAM_MAX_CHAVE];
    chave_para_buf(arv, chave, buf_chave);

    while (i >= 0) {
        void *k = buf_para_chave(arv, folha->chaves[i]);
        int c = arv->cmp(chave, k);
        arv->key_free(k);
        if (c > 0) break;
        memcpy(folha->chaves[i + 1], folha->chaves[i], BPLUS_TAM_MAX_CHAVE);
        folha->dados[i + 1] = folha->dados[i];
        i--;
    }
    memcpy(folha->chaves[i + 1], buf_chave, BPLUS_TAM_MAX_CHAVE);
    folha->dados[i + 1] = offset_dado;
    folha->n_chaves++;
}

/* Insere (chave, filho_direito) num no interno ja lido em memoria, na
 * posicao correta, apos o filho_esquerdo. */
static void inserir_em_interno_ordenado(BPlusTree *arv, NoMem *no, const void *chave, long filho_direito) {
    int i = no->n_chaves - 1;
    unsigned char buf_chave[BPLUS_TAM_MAX_CHAVE];
    chave_para_buf(arv, chave, buf_chave);

    while (i >= 0) {
        void *k = buf_para_chave(arv, no->chaves[i]);
        int c = arv->cmp(chave, k);
        arv->key_free(k);
        if (c > 0) break;
        memcpy(no->chaves[i + 1], no->chaves[i], BPLUS_TAM_MAX_CHAVE);
        no->filhos[i + 2] = no->filhos[i + 1];
        i--;
    }
    memcpy(no->chaves[i + 1], buf_chave, BPLUS_TAM_MAX_CHAVE);
    no->filhos[i + 2] = filho_direito;
    no->n_chaves++;
}

/* Estrutura auxiliar para propagar um split para o nivel superior */
typedef struct {
    int houve_split;
    unsigned char chave_promovida[BPLUS_TAM_MAX_CHAVE];
    long novo_no_direito;
} ResultadoSplit;

static void criar_nova_raiz(BPlusTree *arv, long filho_esq, const unsigned char *chave_buf, long filho_dir) {
    NoMem raiz;
    raiz.self = alocar_bloco(arv);
    raiz.eh_folha = 0;
    raiz.n_chaves = 1;
    memset(raiz.chaves, 0, sizeof(raiz.chaves));
    memcpy(raiz.chaves[0], chave_buf, BPLUS_TAM_MAX_CHAVE);
    memset(raiz.filhos, 0, sizeof(raiz.filhos));
    raiz.filhos[0] = filho_esq;
    raiz.filhos[1] = filho_dir;
    raiz.proxima_folha = BPLUS_NULL;
    gravar_no(arv, &raiz);
    arv->raiz = raiz.self;
}

/* Faz split de uma folha cheia (apos a insercao lógica ela tem BPLUS_ORDEM
 * chaves temporariamente, ou seja, uma acima da capacidade normal). */
static ResultadoSplit split_folha(BPlusTree *arv, NoMem *folha) {
    ResultadoSplit r;
    r.houve_split = 1;

    int total = folha->n_chaves;
    int meio = (total + 1) / 2; /* quantidade que fica na folha esquerda */

    NoMem direita;
    direita.self = alocar_bloco(arv);
    direita.eh_folha = 1;
    direita.n_chaves = total - meio;
    memset(direita.chaves, 0, sizeof(direita.chaves));
    memset(direita.dados, 0, sizeof(direita.dados));

    for (int i = meio; i < total; i++) {
        memcpy(direita.chaves[i - meio], folha->chaves[i], BPLUS_TAM_MAX_CHAVE);
        direita.dados[i - meio] = folha->dados[i];
    }
    direita.proxima_folha = folha->proxima_folha;

    folha->n_chaves = meio;
    folha->proxima_folha = direita.self;

    gravar_no(arv, folha);
    gravar_no(arv, &direita);

    memcpy(r.chave_promovida, direita.chaves[0], BPLUS_TAM_MAX_CHAVE);
    r.novo_no_direito = direita.self;
    return r;
}

/* Faz split de um no interno cheio (apos insercao logica ele tem BPLUS_ORDEM
 * chaves temporariamente). A chave do meio SOBE para o pai e NAO permanece
 * em nenhum dos dois filhos resultantes (semantica classica de B+ para nos
 * internos). */
static ResultadoSplit split_interno(BPlusTree *arv, NoMem *no) {
    ResultadoSplit r;
    r.houve_split = 1;

    int total = no->n_chaves;
    int meio = total / 2;

    memcpy(r.chave_promovida, no->chaves[meio], BPLUS_TAM_MAX_CHAVE);

    NoMem direita;
    direita.self = alocar_bloco(arv);
    direita.eh_folha = 0;
    direita.n_chaves = total - meio - 1;
    memset(direita.chaves, 0, sizeof(direita.chaves));
    memset(direita.filhos, 0, sizeof(direita.filhos));
    direita.proxima_folha = BPLUS_NULL;

    for (int i = meio + 1; i < total; i++) {
        memcpy(direita.chaves[i - meio - 1], no->chaves[i], BPLUS_TAM_MAX_CHAVE);
    }
    for (int i = meio + 1; i <= total; i++) {
        direita.filhos[i - meio - 1] = no->filhos[i];
    }

    no->n_chaves = meio;
    gravar_no(arv, no);
    gravar_no(arv, &direita);

    r.novo_no_direito = direita.self;
    return r;
}

int bplus_inserir(BPlusTree *arv, const void *chave, long offset_dado) {
    /* Arvore vazia: cria raiz-folha */
    if (arv->raiz == BPLUS_NULL) {
        NoMem raiz;
        raiz.self = alocar_bloco(arv);
        raiz.eh_folha = 1;
        raiz.n_chaves = 0;
        memset(raiz.chaves, 0, sizeof(raiz.chaves));
        memset(raiz.dados, 0, sizeof(raiz.dados));
        raiz.proxima_folha = BPLUS_NULL;
        inserir_em_folha_ordenado(arv, &raiz, chave, offset_dado);
        gravar_no(arv, &raiz);
        arv->raiz = raiz.self;
        return 1;
    }

    /* verifica duplicata antes de tudo */
    long dummy;
    if (bplus_buscar(arv, chave, &dummy)) {
        return 0; /* ja existe, nao insere */
    }

    long pilha_offsets[MAX_ALTURA];
    int pilha_idx[MAX_ALTURA];
    int altura;
    long folha_off = descer_para_folha(arv, chave, pilha_offsets, pilha_idx, &altura);

    NoMem folha;
    ler_no(arv, folha_off, &folha);
    inserir_em_folha_ordenado(arv, &folha, chave, offset_dado);

    if (folha.n_chaves <= BPLUS_MAX_CHAVES) {
        gravar_no(arv, &folha);
        return 1;
    }

    /* precisa dar split */
    ResultadoSplit res = split_folha(arv, &folha);

    /* propaga split para os ancestrais */
    int nivel = altura - 1;
    while (res.houve_split && nivel >= 0) {
        long pai_off = pilha_offsets[nivel];
        NoMem pai;
        ler_no(arv, pai_off, &pai);

        void *k = buf_para_chave(arv, res.chave_promovida);
        inserir_em_interno_ordenado(arv, &pai, k, res.novo_no_direito);
        arv->key_free(k);

        if (pai.n_chaves <= BPLUS_MAX_CHAVES) {
            gravar_no(arv, &pai);
            res.houve_split = 0;
        } else {
            res = split_interno(arv, &pai);
        }
        nivel--;
    }

    if (res.houve_split) {
        /* split chegou ate a raiz: cria nova raiz */
        long raiz_antiga = (altura == 0) ? folha.self : pilha_offsets[0];
        criar_nova_raiz(arv, raiz_antiga, res.chave_promovida, res.novo_no_direito);
    }

    return 1;
}

/* ---------------------------------------------------------------------------
 * REMOCAO
 * ---------------------------------------------------------------------------
 * Implementacao com rebalanceamento (emprestimo de irmao ou fusao/merge),
 * respeitando o preenchimento minimo (BPLUS_MIN_CHAVES).
 * ------------------------------------------------------------------------- */

/* Remove a chave de um no folha (assume que ela existe). */
static void remover_de_folha(BPlusTree *arv, NoMem *folha, const void *chave) {
    int pos = -1;
    for (int i = 0; i < folha->n_chaves; i++) {
        void *k = buf_para_chave(arv, folha->chaves[i]);
        int c = arv->cmp(chave, k);
        arv->key_free(k);
        if (c == 0) { pos = i; break; }
    }
    if (pos == -1) return;
    for (int i = pos; i < folha->n_chaves - 1; i++) {
        memcpy(folha->chaves[i], folha->chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
        folha->dados[i] = folha->dados[i + 1];
    }
    folha->n_chaves--;
}

/* Remove a chave na posicao 'pos' de um no interno (removendo tambem o
 * filho a DIREITA dessa chave, indice pos+1). */
static void remover_de_interno(NoMem *no, int pos) {
    for (int i = pos; i < no->n_chaves - 1; i++) {
        memcpy(no->chaves[i], no->chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
    }
    for (int i = pos + 1; i < no->n_chaves; i++) {
        no->filhos[i] = no->filhos[i + 1];
    }
    no->n_chaves--;
}

int bplus_remover(BPlusTree *arv, const void *chave) {
    if (arv->raiz == BPLUS_NULL) return 0;

    long dummy;
    if (!bplus_buscar(arv, chave, &dummy)) return 0;

    long pilha_offsets[MAX_ALTURA];
    int pilha_idx[MAX_ALTURA];
    int altura;
    long folha_off = descer_para_folha(arv, chave, pilha_offsets, pilha_idx, &altura);

    NoMem folha;
    ler_no(arv, folha_off, &folha);
    remover_de_folha(arv, &folha, chave);

    /* raiz eh folha: so grava, sem exigir minimo */
    if (altura == 0) {
        gravar_no(arv, &folha);
        if (folha.n_chaves == 0) {
            /* arvore ficou vazia */
            liberar_bloco(arv, folha.self);
            arv->raiz = BPLUS_NULL;
        }
        return 1;
    }

    if (folha.n_chaves >= BPLUS_MIN_CHAVES) {
        gravar_no(arv, &folha);
        /* pode ser necessario atualizar a chave separadora no ancestral se
         * removemos o menor elemento; a busca funciona corretamente mesmo
         * sem essa atualizacao (B+ tolera chave separadora "desatualizada"
         * pois a comparacao '<' ainda direciona corretamente), entao nao
         * fazemos update para simplificar e manter corretude. */
        return 1;
    }

    /* Underflow na folha: precisa pegar emprestado de um irmao ou fazer merge */
    NoMem atual = folha;
    int nivel = altura - 1;

    while (1) {
        long pai_off = pilha_offsets[nivel];
        int idx_no_pai = pilha_idx[nivel];
        NoMem pai;
        ler_no(arv, pai_off, &pai);

        long irmao_esq_off = (idx_no_pai > 0) ? pai.filhos[idx_no_pai - 1] : BPLUS_NULL;
        long irmao_dir_off = (idx_no_pai < pai.n_chaves) ? pai.filhos[idx_no_pai + 1] : BPLUS_NULL;

        int resolvido = 0;

        /* --- tenta emprestar do irmao esquerdo --- */
        if (!resolvido && irmao_esq_off != BPLUS_NULL) {
            NoMem esq;
            ler_no(arv, irmao_esq_off, &esq);
            if (esq.n_chaves > BPLUS_MIN_CHAVES) {
                if (atual.eh_folha) {
                    /* move ultima chave/dado do irmao esquerdo para o inicio de atual */
                    for (int i = atual.n_chaves; i > 0; i--) {
                        memcpy(atual.chaves[i], atual.chaves[i - 1], BPLUS_TAM_MAX_CHAVE);
                        atual.dados[i] = atual.dados[i - 1];
                    }
                    memcpy(atual.chaves[0], esq.chaves[esq.n_chaves - 1], BPLUS_TAM_MAX_CHAVE);
                    atual.dados[0] = esq.dados[esq.n_chaves - 1];
                    atual.n_chaves++;
                    esq.n_chaves--;

                    /* atualiza chave separadora no pai */
                    memcpy(pai.chaves[idx_no_pai - 1], atual.chaves[0], BPLUS_TAM_MAX_CHAVE);
                } else {
                    /* interno: rotaciona chave e filho */
                    for (int i = atual.n_chaves; i > 0; i--) {
                        memcpy(atual.chaves[i], atual.chaves[i - 1], BPLUS_TAM_MAX_CHAVE);
                    }
                    for (int i = atual.n_chaves + 1; i > 0; i--) {
                        atual.filhos[i] = atual.filhos[i - 1];
                    }
                    memcpy(atual.chaves[0], pai.chaves[idx_no_pai - 1], BPLUS_TAM_MAX_CHAVE);
                    atual.filhos[0] = esq.filhos[esq.n_chaves];
                    atual.n_chaves++;

                    memcpy(pai.chaves[idx_no_pai - 1], esq.chaves[esq.n_chaves - 1], BPLUS_TAM_MAX_CHAVE);
                    esq.n_chaves--;
                }
                gravar_no(arv, &esq);
                gravar_no(arv, &atual);
                gravar_no(arv, &pai);
                resolvido = 1;
            }
        }

        /* --- tenta emprestar do irmao direito --- */
        if (!resolvido && irmao_dir_off != BPLUS_NULL) {
            NoMem dir;
            ler_no(arv, irmao_dir_off, &dir);
            if (dir.n_chaves > BPLUS_MIN_CHAVES) {
                if (atual.eh_folha) {
                    memcpy(atual.chaves[atual.n_chaves], dir.chaves[0], BPLUS_TAM_MAX_CHAVE);
                    atual.dados[atual.n_chaves] = dir.dados[0];
                    atual.n_chaves++;

                    for (int i = 0; i < dir.n_chaves - 1; i++) {
                        memcpy(dir.chaves[i], dir.chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
                        dir.dados[i] = dir.dados[i + 1];
                    }
                    dir.n_chaves--;

                    memcpy(pai.chaves[idx_no_pai], dir.chaves[0], BPLUS_TAM_MAX_CHAVE);
                } else {
                    memcpy(atual.chaves[atual.n_chaves], pai.chaves[idx_no_pai], BPLUS_TAM_MAX_CHAVE);
                    atual.filhos[atual.n_chaves + 1] = dir.filhos[0];
                    atual.n_chaves++;

                    memcpy(pai.chaves[idx_no_pai], dir.chaves[0], BPLUS_TAM_MAX_CHAVE);

                    for (int i = 0; i < dir.n_chaves - 1; i++) {
                        memcpy(dir.chaves[i], dir.chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
                    }
                    for (int i = 0; i < dir.n_chaves; i++) {
                        dir.filhos[i] = dir.filhos[i + 1];
                    }
                    dir.n_chaves--;
                }
                gravar_no(arv, &dir);
                gravar_no(arv, &atual);
                gravar_no(arv, &pai);
                resolvido = 1;
            }
        }

        if (resolvido) return 1;

        /* --- nao foi possivel emprestar: faz merge (fusao) --- */
        if (irmao_esq_off != BPLUS_NULL) {
            /* funde 'atual' com irmao esquerdo -> esquerdo absorve atual */
            NoMem esq;
            ler_no(arv, irmao_esq_off, &esq);

            if (atual.eh_folha) {
                for (int i = 0; i < atual.n_chaves; i++) {
                    memcpy(esq.chaves[esq.n_chaves + i], atual.chaves[i], BPLUS_TAM_MAX_CHAVE);
                    esq.dados[esq.n_chaves + i] = atual.dados[i];
                }
                esq.n_chaves += atual.n_chaves;
                esq.proxima_folha = atual.proxima_folha;
            } else {
                memcpy(esq.chaves[esq.n_chaves], pai.chaves[idx_no_pai - 1], BPLUS_TAM_MAX_CHAVE);
                for (int i = 0; i < atual.n_chaves; i++) {
                    memcpy(esq.chaves[esq.n_chaves + 1 + i], atual.chaves[i], BPLUS_TAM_MAX_CHAVE);
                }
                for (int i = 0; i <= atual.n_chaves; i++) {
                    esq.filhos[esq.n_chaves + 1 + i] = atual.filhos[i];
                }
                esq.n_chaves += atual.n_chaves + 1;
            }
            gravar_no(arv, &esq);
            liberar_bloco(arv, atual.self);
            remover_de_interno(&pai, idx_no_pai - 1);
        } else {
            /* funde irmao direito em 'atual' -> atual absorve direito */
            NoMem dir;
            ler_no(arv, irmao_dir_off, &dir);

            if (atual.eh_folha) {
                for (int i = 0; i < dir.n_chaves; i++) {
                    memcpy(atual.chaves[atual.n_chaves + i], dir.chaves[i], BPLUS_TAM_MAX_CHAVE);
                    atual.dados[atual.n_chaves + i] = dir.dados[i];
                }
                atual.n_chaves += dir.n_chaves;
                atual.proxima_folha = dir.proxima_folha;
            } else {
                memcpy(atual.chaves[atual.n_chaves], pai.chaves[idx_no_pai], BPLUS_TAM_MAX_CHAVE);
                for (int i = 0; i < dir.n_chaves; i++) {
                    memcpy(atual.chaves[atual.n_chaves + 1 + i], dir.chaves[i], BPLUS_TAM_MAX_CHAVE);
                }
                for (int i = 0; i <= dir.n_chaves; i++) {
                    atual.filhos[atual.n_chaves + 1 + i] = dir.filhos[i];
                }
                atual.n_chaves += dir.n_chaves + 1;
            }
            gravar_no(arv, &atual);
            liberar_bloco(arv, dir.self);
            remover_de_interno(&pai, idx_no_pai);
        }

        /* pai perdeu uma chave; verifica se pai ainda satisfaz minimo */
        if (nivel == 0) {
            /* pai eh a raiz */
            if (pai.n_chaves == 0) {
                /* raiz ficou vazia: seu unico filho remanescente vira a nova raiz */
                arv->raiz = pai.filhos[0];
                liberar_bloco(arv, pai.self);
            } else {
                gravar_no(arv, &pai);
            }
            return 1;
        }

        if (pai.n_chaves >= BPLUS_MIN_CHAVES) {
            gravar_no(arv, &pai);
            return 1;
        }

        /* pai tambem esta em underflow: sobe um nivel e repete o processo */
        gravar_no(arv, &pai);
        atual = pai;
        nivel--;
    }
}

/* ---------------------------------------------------------------------------
 * BUSCA POR INTERVALO E PERCURSO COMPLETO
 * ------------------------------------------------------------------------- */

static long encontrar_primeira_folha(BPlusTree *arv) {
    if (arv->raiz == BPLUS_NULL) return BPLUS_NULL;
    long atual = arv->raiz;
    while (1) {
        NoMem no;
        ler_no(arv, atual, &no);
        if (no.eh_folha) return atual;
        atual = no.filhos[0];
    }
}

void bplus_percorrer(BPlusTree *arv, bplus_visit_fn visitar, void *ctx) {
    long folha_off = encontrar_primeira_folha(arv);
    while (folha_off != BPLUS_NULL) {
        NoMem folha;
        ler_no(arv, folha_off, &folha);
        for (int i = 0; i < folha.n_chaves; i++) {
            void *k = buf_para_chave(arv, folha.chaves[i]);
            visitar(k, folha.dados[i], ctx);
            arv->key_free(k);
        }
        folha_off = folha.proxima_folha;
    }
}

void bplus_buscar_intervalo(BPlusTree *arv, const void *chaveA, const void *chaveB,
                             bplus_visit_fn visitar, void *ctx) {
    if (arv->raiz == BPLUS_NULL) return;

    /* desce até a folha onde chaveA comecaria */
    long folha_off = descer_para_folha(arv, chaveA, NULL, NULL, NULL);

    while (folha_off != BPLUS_NULL) {
        NoMem folha;
        ler_no(arv, folha_off, &folha);
        for (int i = 0; i < folha.n_chaves; i++) {
            void *k = buf_para_chave(arv, folha.chaves[i]);
            int cA = arv->cmp(k, chaveA);
            int cB = arv->cmp(k, chaveB);
            if (cA > 0 && cB < 0) {
                visitar(k, folha.dados[i], ctx);
            }
            arv->key_free(k);
            if (cB >= 0) {
                /* ja passamos do limite superior: pode parar tudo */
                return;
            }
        }
        folha_off = folha.proxima_folha;
    }
}

/* ---------------------------------------------------------------------------
 * IMPRESSAO HIERARQUICA DA ESTRUTURA
 * ------------------------------------------------------------------------- */

static void imprimir_no_recursivo(BPlusTree *arv, long offset, int nivel) {
    if (offset == BPLUS_NULL) return;
    NoMem no;
    ler_no(arv, offset, &no);

    for (int t = 0; t < nivel; t++) printf("    ");
    printf("%s[off=%ld] (", no.eh_folha ? "FOLHA " : "NO-INT", offset);
    for (int i = 0; i < no.n_chaves; i++) {
        void *k = buf_para_chave(arv, no.chaves[i]);
        if (i > 0) printf(" | ");
        arv->key_print(k);
        arv->key_free(k);
    }
    printf(")\n");

    if (!no.eh_folha) {
        for (int i = 0; i <= no.n_chaves; i++) {
            imprimir_no_recursivo(arv, no.filhos[i], nivel + 1);
        }
    }
}

void bplus_imprimir_estrutura(BPlusTree *arv) {
    if (arv->raiz == BPLUS_NULL) {
        printf("(arvore vazia)\n");
        return;
    }
    printf("=== Estrutura da Arvore B+ (raiz no offset %ld) ===\n", arv->raiz);
    imprimir_no_recursivo(arv, arv->raiz, 0);
    printf("===================================================\n");
}
