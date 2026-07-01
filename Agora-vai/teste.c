/* ============================================================================
 * bplus.c
 *
 * Implementacao da Arvore B+ generica em disco (Refatorada).
 * ==========================================================================*/

 #include "bplus.h"
 #include <string.h>
 #include <assert.h>
 
 #define OFFSET_CABECALHO 0L
 #define OFFSET_PRIMEIRO_NO ((long)sizeof(Cabecalho))
 #define MAX_ALTURA 64
 
 /* --- ESTRUTURAS DE DISCO (Tamanho Fixo para fread/fwrite) --- */
 typedef struct {
     long raiz;
     long topo;
 } Cabecalho;
 
 typedef struct {
     int eh_folha;
     int n_chaves;
     unsigned char chaves[BPLUS_MAX_CHAVES][BPLUS_TAM_MAX_CHAVE];
     long ponteiros[BPLUS_ORDEM]; /* Compartilhado: Filhos (no interno) OU Dados+Proxima (folha) */
 } NoDisco;
 
 /* --- ESTRUTURA DE MEMORIA (Com +1 de capacidade para os splits lógicos) --- */
 typedef struct {
     long self;
     int eh_folha;
     int n_chaves;
     unsigned char chaves[BPLUS_MAX_CHAVES + 1][BPLUS_TAM_MAX_CHAVE];
     long filhos[BPLUS_ORDEM + 1];
     long dados[BPLUS_MAX_CHAVES + 1];
     long proxima_folha;
 } NoMem;
 
 /* ---------------------------------------------------------------------------
  * E/S FISICA (Serializacao transparente sem memcpy byte-a-byte)
  * ------------------------------------------------------------------------- */
 static void ler_cabecalho(BPlusTree *arv, Cabecalho *cab) {
     fseek(arv->arquivo, OFFSET_CABECALHO, SEEK_SET);
     fread(cab, sizeof(Cabecalho), 1, arv->arquivo);
 }
 
 static void gravar_cabecalho(BPlusTree *arv) {
     Cabecalho cab = { arv->raiz, arv->topo };
     fseek(arv->arquivo, OFFSET_CABECALHO, SEEK_SET);
     fwrite(&cab, sizeof(Cabecalho), 1, arv->arquivo);
     fflush(arv->arquivo);
 }
 
 static void ler_no(BPlusTree *arv, long offset, NoMem *no) {
     NoDisco nd;
     fseek(arv->arquivo, offset, SEEK_SET);
     fread(&nd, sizeof(NoDisco), 1, arv->arquivo);
     
     no->self = offset;
     no->eh_folha = nd.eh_folha;
     no->n_chaves = nd.n_chaves;
     no->proxima_folha = BPLUS_NULL;
     memset(no->filhos, 0, sizeof(no->filhos));
     memset(no->dados, 0, sizeof(no->dados));
 
     for (int i = 0; i < nd.n_chaves; i++) {
         memcpy(no->chaves[i], nd.chaves[i], BPLUS_TAM_MAX_CHAVE);
     }
     if (nd.eh_folha) {
         for (int i = 0; i < nd.n_chaves; i++) no->dados[i] = nd.ponteiros[i];
         no->proxima_folha = nd.ponteiros[BPLUS_ORDEM - 1];
     } else {
         for (int i = 0; i <= nd.n_chaves; i++) no->filhos[i] = nd.ponteiros[i];
     }
 }
 
 static void gravar_no(BPlusTree *arv, const NoMem *no) {
     NoDisco nd;
     memset(&nd, 0, sizeof(NoDisco));
     nd.eh_folha = no->eh_folha;
     nd.n_chaves = no->n_chaves;
 
     for (int i = 0; i < no->n_chaves; i++) {
         memcpy(nd.chaves[i], no->chaves[i], BPLUS_TAM_MAX_CHAVE);
     }
     if (no->eh_folha) {
         for (int i = 0; i < no->n_chaves; i++) nd.ponteiros[i] = no->dados[i];
         nd.ponteiros[BPLUS_ORDEM - 1] = no->proxima_folha;
     } else {
         for (int i = 0; i <= no->n_chaves; i++) nd.ponteiros[i] = no->filhos[i];
     }
 
     fseek(arv->arquivo, no->self, SEEK_SET);
     fwrite(&nd, sizeof(NoDisco), 1, arv->arquivo);
     fflush(arv->arquivo);
 }
 
 /* Espaco: Crescimento Linear simples (Sem reaproveitamento de blocos mortos) */
 static long alocar_bloco(BPlusTree *arv) {
     long offset = arv->topo;
     arv->topo += sizeof(NoDisco);
     return offset;
 }
 
 /* ---------------------------------------------------------------------------
  * UTILITARIOS
  * ------------------------------------------------------------------------- */
 static void chave_para_buf(BPlusTree *arv, const void *chave, unsigned char *buf) {
     memset(buf, 0, BPLUS_TAM_MAX_CHAVE);
     arv->key_write(chave, buf);
 }
 
 static void *buf_para_chave(BPlusTree *arv, const unsigned char *buf) {
     return arv->key_read(buf);
 }
 
 /* ---------------------------------------------------------------------------
  * CICLO DE VIDA DA ARVORE
  * ------------------------------------------------------------------------- */
 BPlusTree *bplus_criar(const char *caminho, bplus_cmp_fn cmp, bplus_key_write_fn kw, bplus_key_read_fn kr, bplus_key_size_fn ks, bplus_key_free_fn kf, bplus_key_print_fn kp) {
     BPlusTree *arv = (BPlusTree *)malloc(sizeof(BPlusTree));
     strncpy(arv->caminho, caminho, 255);
     arv->cmp = cmp; arv->key_write = kw; arv->key_read = kr; arv->key_size = ks; arv->key_free = kf; arv->key_print = kp;
     arv->tam_chave = ks();
     
     arv->arquivo = fopen(caminho, "wb+");
     arv->raiz = BPLUS_NULL;
     arv->topo = OFFSET_PRIMEIRO_NO;
     gravar_cabecalho(arv);
     return arv;
 }
 
 BPlusTree *bplus_abrir(const char *caminho, bplus_cmp_fn cmp, bplus_key_write_fn kw, bplus_key_read_fn kr, bplus_key_size_fn ks, bplus_key_free_fn kf, bplus_key_print_fn kp) {
     FILE *teste = fopen(caminho, "rb");
     if (!teste) return bplus_criar(caminho, cmp, kw, kr, ks, kf, kp);
     fclose(teste);
 
     BPlusTree *arv = (BPlusTree *)malloc(sizeof(BPlusTree));
     strncpy(arv->caminho, caminho, 255);
     arv->cmp = cmp; arv->key_write = kw; arv->key_read = kr; arv->key_size = ks; arv->key_free = kf; arv->key_print = kp;
     arv->tam_chave = ks();
 
     arv->arquivo = fopen(caminho, "rb+");
     Cabecalho cab;
     ler_cabecalho(arv, &cab);
     arv->raiz = cab.raiz;
     arv->topo = cab.topo;
     return arv;
 }
 
 void bplus_fechar(BPlusTree *arv) {
     if (!arv) return;
     gravar_cabecalho(arv);
     fclose(arv->arquivo);
     free(arv);
 }
 
 /* ---------------------------------------------------------------------------
  * NAVEGACAO E BUSCA
  * ------------------------------------------------------------------------- */
 static long descer_para_folha(BPlusTree *arv, const void *chave, long *pilha_offsets, int *pilha_idx, int *altura_saida) {
     long atual = arv->raiz;
     int altura = 0;
     while (1) {
         NoMem no;
         ler_no(arv, atual, &no);
         if (no.eh_folha) {
             if (altura_saida) *altura_saida = altura;
             return atual;
         }
         int i = 0;
         while (i < no.n_chaves) {
             void *k = buf_para_chave(arv, no.chaves[i]);
             int c = arv->cmp(chave, k);
             arv->key_free(k);
             if (c < 0) break;
             i++;
         }
         if (pilha_offsets) pilha_offsets[altura] = atual;
         if (pilha_idx) pilha_idx[altura] = i;
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
 static void inserir_em_folha(BPlusTree *arv, NoMem *folha, const void *chave, long offset) {
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
     folha->dados[i + 1] = offset;
     folha->n_chaves++;
 }
 
 static void inserir_em_interno(BPlusTree *arv, NoMem *no, const void *chave, long filho_dir) {
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
     no->filhos[i + 2] = filho_dir;
     no->n_chaves++;
 }
 
 int bplus_inserir(BPlusTree *arv, const void *chave, long offset_dado) {
     if (arv->raiz == BPLUS_NULL) {
         NoMem raiz;
         memset(&raiz, 0, sizeof(NoMem));
         raiz.self = alocar_bloco(arv);
         raiz.eh_folha = 1;
         raiz.proxima_folha = BPLUS_NULL;
         inserir_em_folha(arv, &raiz, chave, offset_dado);
         gravar_no(arv, &raiz);
         arv->raiz = raiz.self;
         return 1;
     }
 
     if (bplus_buscar(arv, chave, NULL)) return 0; /* Duplicata */
 
     long pilha_offsets[MAX_ALTURA];
     int pilha_idx[MAX_ALTURA];
     int altura;
     long folha_off = descer_para_folha(arv, chave, pilha_offsets, pilha_idx, &altura);
 
     NoMem folha;
     ler_no(arv, folha_off, &folha);
     inserir_em_folha(arv, &folha, chave, offset_dado);
 
     if (folha.n_chaves <= BPLUS_MAX_CHAVES) {
         gravar_no(arv, &folha);
         return 1;
     }
 
     /* Processo de Split Isolado */
     int nivel = altura;
     NoMem atual = folha;
     int split = 1;
     unsigned char promovida[BPLUS_TAM_MAX_CHAVE];
     long filho_direito_novo = BPLUS_NULL;
 
     while (split && nivel >= 0) {
         int meio = atual.eh_folha ? (atual.n_chaves + 1) / 2 : atual.n_chaves / 2;
         
         NoMem dir;
         memset(&dir, 0, sizeof(NoMem));
         dir.self = alocar_bloco(arv);
         dir.eh_folha = atual.eh_folha;
         dir.proxima_folha = atual.proxima_folha;
 
         if (atual.eh_folha) {
             dir.n_chaves = atual.n_chaves - meio;
             for (int i = meio; i < atual.n_chaves; i++) {
                 memcpy(dir.chaves[i - meio], atual.chaves[i], BPLUS_TAM_MAX_CHAVE);
                 dir.dados[i - meio] = atual.dados[i];
             }
             atual.n_chaves = meio;
             atual.proxima_folha = dir.self;
             memcpy(promovida, dir.chaves[0], BPLUS_TAM_MAX_CHAVE);
         } else {
             memcpy(promovida, atual.chaves[meio], BPLUS_TAM_MAX_CHAVE);
             dir.n_chaves = atual.n_chaves - meio - 1;
             for (int i = meio + 1; i < atual.n_chaves; i++) {
                 memcpy(dir.chaves[i - meio - 1], atual.chaves[i], BPLUS_TAM_MAX_CHAVE);
             }
             for (int i = meio + 1; i <= atual.n_chaves; i++) {
                 dir.filhos[i - meio - 1] = atual.filhos[i];
             }
             atual.n_chaves = meio;
         }
 
         gravar_no(arv, &atual);
         gravar_no(arv, &dir);
         filho_direito_novo = dir.self;
 
         nivel--;
         if (nivel >= 0) {
             ler_no(arv, pilha_offsets[nivel], &atual);
             void *k = buf_para_chave(arv, promovida);
             inserir_em_interno(arv, &atual, k, filho_direito_novo);
             arv->key_free(k);
             
             if (atual.n_chaves <= BPLUS_MAX_CHAVES) {
                 gravar_no(arv, &atual);
                 split = 0;
             }
         }
     }
 
     if (split) {
         NoMem nova_raiz;
         memset(&nova_raiz, 0, sizeof(NoMem));
         nova_raiz.self = alocar_bloco(arv);
         nova_raiz.eh_folha = 0;
         nova_raiz.n_chaves = 1;
         memcpy(nova_raiz.chaves[0], promovida, BPLUS_TAM_MAX_CHAVE);
         nova_raiz.filhos[0] = (altura == 0) ? folha.self : pilha_offsets[0];
         nova_raiz.filhos[1] = filho_direito_novo;
         gravar_no(arv, &nova_raiz);
         arv->raiz = nova_raiz.self;
     }
     return 1;
 }
 
 /* ---------------------------------------------------------------------------
  * REMOCAO E REBALANCEAMENTO (Simplificado por Auxiliares)
  * ------------------------------------------------------------------------- */
 static int efetuar_emprestimo_esq(BPlusTree *arv, NoMem *atual, NoMem *pai, int idx, long off_esq) {
     if (off_esq == BPLUS_NULL) return 0;
     NoMem esq; ler_no(arv, off_esq, &esq);
     if (esq.n_chaves <= BPLUS_MIN_CHAVES) return 0; /* Nao pode emprestar */
 
     if (atual->eh_folha) {
         for (int i = atual->n_chaves; i > 0; i--) {
             memcpy(atual->chaves[i], atual->chaves[i - 1], BPLUS_TAM_MAX_CHAVE);
             atual->dados[i] = atual->dados[i - 1];
         }
         memcpy(atual->chaves[0], esq.chaves[esq.n_chaves - 1], BPLUS_TAM_MAX_CHAVE);
         atual->dados[0] = esq.dados[esq.n_chaves - 1];
         memcpy(pai->chaves[idx - 1], atual->chaves[0], BPLUS_TAM_MAX_CHAVE);
     } else {
         for (int i = atual->n_chaves; i > 0; i--) memcpy(atual->chaves[i], atual->chaves[i - 1], BPLUS_TAM_MAX_CHAVE);
         for (int i = atual->n_chaves + 1; i > 0; i--) atual->filhos[i] = atual->filhos[i - 1];
         memcpy(atual->chaves[0], pai->chaves[idx - 1], BPLUS_TAM_MAX_CHAVE);
         atual->filhos[0] = esq.filhos[esq.n_chaves];
         memcpy(pai->chaves[idx - 1], esq.chaves[esq.n_chaves - 1], BPLUS_TAM_MAX_CHAVE);
     }
     atual->n_chaves++;
     esq.n_chaves--;
     gravar_no(arv, &esq); gravar_no(arv, atual); gravar_no(arv, pai);
     return 1;
 }
 
 static int efetuar_emprestimo_dir(BPlusTree *arv, NoMem *atual, NoMem *pai, int idx, long off_dir) {
     if (off_dir == BPLUS_NULL) return 0;
     NoMem dir; ler_no(arv, off_dir, &dir);
     if (dir.n_chaves <= BPLUS_MIN_CHAVES) return 0; /* Nao pode emprestar */
 
     if (atual->eh_folha) {
         memcpy(atual->chaves[atual->n_chaves], dir.chaves[0], BPLUS_TAM_MAX_CHAVE);
         atual->dados[atual->n_chaves] = dir.dados[0];
         memcpy(pai->chaves[idx], dir.chaves[1], BPLUS_TAM_MAX_CHAVE);
         for (int i = 0; i < dir.n_chaves - 1; i++) {
             memcpy(dir.chaves[i], dir.chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
             dir.dados[i] = dir.dados[i + 1];
         }
     } else {
         memcpy(atual->chaves[atual->n_chaves], pai->chaves[idx], BPLUS_TAM_MAX_CHAVE);
         atual->filhos[atual->n_chaves + 1] = dir.filhos[0];
         memcpy(pai->chaves[idx], dir.chaves[0], BPLUS_TAM_MAX_CHAVE);
         for (int i = 0; i < dir.n_chaves - 1; i++) memcpy(dir.chaves[i], dir.chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
         for (int i = 0; i < dir.n_chaves; i++) dir.filhos[i] = dir.filhos[i + 1];
     }
     atual->n_chaves++;
     dir.n_chaves--;
     gravar_no(arv, &dir); gravar_no(arv, atual); gravar_no(arv, pai);
     return 1;
 }
 
 static void fundir_nos(BPlusTree *arv, NoMem *esq, NoMem *dir, NoMem *pai, int idx_separador) {
     if (esq->eh_folha) {
         for (int i = 0; i < dir->n_chaves; i++) {
             memcpy(esq->chaves[esq->n_chaves + i], dir->chaves[i], BPLUS_TAM_MAX_CHAVE);
             esq->dados[esq->n_chaves + i] = dir->dados[i];
         }
         esq->proxima_folha = dir->proxima_folha;
     } else {
         memcpy(esq->chaves[esq->n_chaves], pai->chaves[idx_separador], BPLUS_TAM_MAX_CHAVE);
         for (int i = 0; i < dir->n_chaves; i++) {
             memcpy(esq->chaves[esq->n_chaves + 1 + i], dir->chaves[i], BPLUS_TAM_MAX_CHAVE);
         }
         for (int i = 0; i <= dir->n_chaves; i++) {
             esq->filhos[esq->n_chaves + 1 + i] = dir->filhos[i];
         }
         esq->n_chaves++; /* Soma extra pela chave que desceu do pai */
     }
     esq->n_chaves += dir->n_chaves;
     gravar_no(arv, esq);
     /* dir se torna bloco orfão (ignorado fisicamente p/ simplicidade linear) */
     
     /* Remove separador do pai */
     for (int i = idx_separador; i < pai->n_chaves - 1; i++) {
         memcpy(pai->chaves[i], pai->chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
     }
     for (int i = idx_separador + 1; i < pai->n_chaves; i++) {
         pai->filhos[i] = pai->filhos[i + 1];
     }
     pai->n_chaves--;
 }
 
 int bplus_remover(BPlusTree *arv, const void *chave) {
     if (arv->raiz == BPLUS_NULL) return 0;
     if (!bplus_buscar(arv, chave, NULL)) return 0;
 
     long p_off[MAX_ALTURA];
     int p_idx[MAX_ALTURA];
     int altura;
     long folha_off = descer_para_folha(arv, chave, p_off, p_idx, &altura);
 
     NoMem atual;
     ler_no(arv, folha_off, &atual);
 
     /* Encontra e remove na folha */
     int pos = -1;
     for (int i = 0; i < atual.n_chaves; i++) {
         void *k = buf_para_chave(arv, atual.chaves[i]);
         if (arv->cmp(chave, k) == 0) pos = i;
         arv->key_free(k);
         if (pos != -1) break;
     }
     for (int i = pos; i < atual.n_chaves - 1; i++) {
         memcpy(atual.chaves[i], atual.chaves[i + 1], BPLUS_TAM_MAX_CHAVE);
         atual.dados[i] = atual.dados[i + 1];
     }
     atual.n_chaves--;
 
     int nivel = altura - 1;
     while (1) {
         if (nivel < 0) {
             /* Chegou na raiz */
             if (atual.n_chaves == 0 && !atual.eh_folha) arv->raiz = atual.filhos[0];
             else if (atual.n_chaves == 0 && atual.eh_folha) arv->raiz = BPLUS_NULL;
             else gravar_no(arv, &atual);
             return 1;
         }
 
         if (atual.n_chaves >= BPLUS_MIN_CHAVES) {
             gravar_no(arv, &atual);
             return 1;
         }
 
         NoMem pai; ler_no(arv, p_off[nivel], &pai);
         int idx = p_idx[nivel];
         long esq_off = (idx > 0) ? pai.filhos[idx - 1] : BPLUS_NULL;
         long dir_off = (idx < pai.n_chaves) ? pai.filhos[idx + 1] : BPLUS_NULL;
 
         if (efetuar_emprestimo_esq(arv, &atual, &pai, idx, esq_off)) return 1;
         if (efetuar_emprestimo_dir(arv, &atual, &pai, idx, dir_off)) return 1;
 
         /* Merge */
         if (esq_off != BPLUS_NULL) {
             NoMem esq; ler_no(arv, esq_off, &esq);
             fundir_nos(arv, &esq, &atual, &pai, idx - 1);
         } else {
             NoMem dir; ler_no(arv, dir_off, &dir);
             fundir_nos(arv, &atual, &dir, &pai, idx);
         }
 
         atual = pai;
         nivel--;
     }
 }
 
 /* ---------------------------------------------------------------------------
  * PERCURSO E EXIBICAO
  * ------------------------------------------------------------------------- */
 void bplus_percorrer(BPlusTree *arv, bplus_visit_fn visitar, void *ctx) {
     if (arv->raiz == BPLUS_NULL) return;
     long folha_off = arv->raiz;
     while (1) {
         NoMem no; ler_no(arv, folha_off, &no);
         if (no.eh_folha) break;
         folha_off = no.filhos[0];
     }
     while (folha_off != BPLUS_NULL) {
         NoMem folha; ler_no(arv, folha_off, &folha);
         for (int i = 0; i < folha.n_chaves; i++) {
             void *k = buf_para_chave(arv, folha.chaves[i]);
             visitar(k, folha.dados[i], ctx);
             arv->key_free(k);
         }
         folha_off = folha.proxima_folha;
     }
 }
 
 void bplus_buscar_intervalo(BPlusTree *arv, const void *chaveA, const void *chaveB, bplus_visit_fn visitar, void *ctx) {
     if (arv->raiz == BPLUS_NULL) return;
     long folha_off = descer_para_folha(arv, chaveA, NULL, NULL, NULL);
     while (folha_off != BPLUS_NULL) {
         NoMem folha; ler_no(arv, folha_off, &folha);
         for (int i = 0; i < folha.n_chaves; i++) {
             void *k = buf_para_chave(arv, folha.chaves[i]);
             if (arv->cmp(k, chaveA) > 0 && arv->cmp(k, chaveB) < 0) visitar(k, folha.dados[i], ctx);
             int cond = arv->cmp(k, chaveB) >= 0;
             arv->key_free(k);
             if (cond) return;
         }
         folha_off = folha.proxima_folha;
     }
 }
 
 static void imprimir_recursivo(BPlusTree *arv, long off, int nivel) {
     if (off == BPLUS_NULL) return;
     NoMem no; ler_no(arv, off, &no);
     for (int t = 0; t < nivel; t++) printf("    ");
     printf("%s[off=%ld] (", no.eh_folha ? "FOLHA " : "NO-INT", off);
     for (int i = 0; i < no.n_chaves; i++) {
         void *k = buf_para_chave(arv, no.chaves[i]);
         if (i > 0) printf(" | ");
         arv->key_print(k);
         arv->key_free(k);
     }
     printf(")\n");
     if (!no.eh_folha) {
         for (int i = 0; i <= no.n_chaves; i++) imprimir_recursivo(arv, no.filhos[i], nivel + 1);
     }
 }
 
 void bplus_imprimir_estrutura(BPlusTree *arv) {
     if (arv->raiz == BPLUS_NULL) printf("(arvore vazia)\n");
     else imprimir_recursivo(arv, arv->raiz, 0);
 }