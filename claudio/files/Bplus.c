#include "Bplus.h"

/* ============================================================================
 * ESTRUTURA DA PÁGINA (NÓ) NA MEMÓRIA RAM
 * Usada apenas temporariamente para o nó que está sendo manipulado.
 * ========================================================================= */
typedef struct {
    long endereco_proprio;    // Onde esta página está salva no arquivo
    bool eh_folha;            // 1 se for folha, 0 se for nó interno
    int qtd_elementos;        // Quantidade atual de chaves
    
    // Matriz de bytes para guardar chaves genéricas (serializadas)
    unsigned char chaves[MAX_CHAVES + 1][TAM_MAX_CHAVE]; 
    
    // Se for nó interno, guarda os endereços das páginas filhas
    long filhos[ORDEM + 1];                 
    
    // Se for folha, guarda o endereço do registro no arquivo de dados do RH
    long registros_dados[MAX_CHAVES + 1];   
    long proxima_folha;       // Lista encadeada de folhas
} Pagina;

/* ============================================================================
 * CABEÇALHO DO ARQUIVO BINÁRIO
 * ========================================================================= */
typedef struct {
    long raiz;
    long proximo_bloco_livre;
} CabecalhoDisco;

typedef struct {
    bool houve_split;
    unsigned char chave_promovida[TAM_MAX_CHAVE];
    long novo_filho_direito;
} ResultadoSplit;

/* ============================================================================
 * FUNÇÕES DE DISCO (FSEEK, FREAD, FWRITE)
 * Garantem a persistência exigida no PDF.
 * ========================================================================= */
static long calcular_tamanho_pagina() {
    long tamanho_base = sizeof(bool) + sizeof(int); // eh_folha + qtd_elementos
    tamanho_base += (MAX_CHAVES * TAM_MAX_CHAVE); // Espaço das chaves
    
    long tamanho_interno = tamanho_base + (sizeof(long) * ORDEM); // Filhos
    long tamanho_folha = tamanho_base + (sizeof(long) * MAX_CHAVES) + sizeof(long); // Registros + prox folha
    
    return (tamanho_interno > tamanho_folha) ? tamanho_interno : tamanho_folha;
}

// Grava a página no disco transformando a struct Pagina em um bloco de bytes
static void salvar_pagina_disco(ArvoreBPlus *arv, const Pagina *pag) {
    unsigned char *buffer = (unsigned char *)calloc(1, arv->tamanho_no_bytes);
    unsigned char *cursor = buffer;
    
    memcpy(cursor, &pag->eh_folha, sizeof(bool)); cursor += sizeof(bool);
    memcpy(cursor, &pag->qtd_elementos, sizeof(int)); cursor += sizeof(int);
    memcpy(cursor, pag->chaves, MAX_CHAVES * TAM_MAX_CHAVE); cursor += (MAX_CHAVES * TAM_MAX_CHAVE);
    
    if (pag->eh_folha) {
        memcpy(cursor, pag->registros_dados, sizeof(long) * MAX_CHAVES); cursor += sizeof(long) * MAX_CHAVES;
        memcpy(cursor, &pag->proxima_folha, sizeof(long));
    } else {
        memcpy(cursor, pag->filhos, sizeof(long) * ORDEM);
    }
    
    fseek(arv->arquivo_disco, pag->endereco_proprio, SEEK_SET);
    fwrite(buffer, arv->tamanho_no_bytes, 1, arv->arquivo_disco);
    fflush(arv->arquivo_disco);
    free(buffer);
}

// Lê um bloco de bytes do disco e preenche a struct Pagina
static void carregar_pagina_disco(ArvoreBPlus *arv, long endereco, Pagina *pag) {
    unsigned char *buffer = (unsigned char *)malloc(arv->tamanho_no_bytes);
    
    fseek(arv->arquivo_disco, endereco, SEEK_SET);
    fread(buffer, arv->tamanho_no_bytes, 1, arv->arquivo_disco);
    
    unsigned char *cursor = buffer;
    memcpy(&pag->eh_folha, cursor, sizeof(bool)); cursor += sizeof(bool);
    memcpy(&pag->qtd_elementos, cursor, sizeof(int)); cursor += sizeof(int);
    memcpy(pag->chaves, cursor, MAX_CHAVES * TAM_MAX_CHAVE); cursor += (MAX_CHAVES * TAM_MAX_CHAVE);
    
    if (pag->eh_folha) {
        memcpy(pag->registros_dados, cursor, sizeof(long) * MAX_CHAVES); cursor += sizeof(long) * MAX_CHAVES;
        memcpy(&pag->proxima_folha, cursor, sizeof(long));
        memset(pag->filhos, 0, sizeof(pag->filhos));
    } else {
        memcpy(pag->filhos, cursor, sizeof(long) * ORDEM);
        memset(pag->registros_dados, 0, sizeof(pag->registros_dados));
        pag->proxima_folha = ENDERECO_NULO;
    }
    
    pag->endereco_proprio = endereco;
    free(buffer);
}

static void atualizar_cabecalho(ArvoreBPlus *arv) {
    CabecalhoDisco cab = { arv->endereco_raiz, arv->proximo_bloco_livre };
    fseek(arv->arquivo_disco, 0, SEEK_SET);
    fwrite(&cab, sizeof(CabecalhoDisco), 1, arv->arquivo_disco);
    fflush(arv->arquivo_disco);
}

/* ============================================================================
 * INICIALIZAÇÃO DA ÁRVORE
 * ========================================================================= */
ArvoreBPlus* criar_arvore(const char *caminho, 
                          CompararChavesFn cmp, SerializarChaveFn ser, 
                          DeserializarChaveFn des, TamanhoChaveFn tam, 
                          LiberarChaveFn lib, ImprimirChaveFn imp) {
    
    ArvoreBPlus *arv = (ArvoreBPlus *)malloc(sizeof(ArvoreBPlus));
    arv->comparar = cmp;
    arv->serializar = ser;
    arv->deserializar = des;
    arv->tamanho = tam;
    arv->liberar = lib;
    arv->imprimir = imp;
    arv->tamanho_no_bytes = calcular_tamanho_pagina();

    FILE *teste = fopen(caminho, "rb");
    if (teste) {
        // Arquivo já existe, apenas carrega o cabeçalho
        fclose(teste);
        arv->arquivo_disco = fopen(caminho, "rb+");
        CabecalhoDisco cab;
        fseek(arv->arquivo_disco, 0, SEEK_SET);
        fread(&cab, sizeof(CabecalhoDisco), 1, arv->arquivo_disco);
        arv->endereco_raiz = cab.raiz;
        arv->proximo_bloco_livre = cab.proximo_bloco_livre;
    } else {
        // Cria um arquivo novo
        arv->arquivo_disco = fopen(caminho, "wb+");
        arv->endereco_raiz = ENDERECO_NULO;
        arv->proximo_bloco_livre = sizeof(CabecalhoDisco);
        atualizar_cabecalho(arv);
    }
    
    return arv;
}

void fechar_arvore(ArvoreBPlus *arv) {
    if (!arv) return;
    atualizar_cabecalho(arv);
    fclose(arv->arquivo_disco);
    free(arv);
}

/* ============================================================================
 * EXEMPLO DE LÓGICA DE BUSCA (Usando o padrão limpo)
 * ========================================================================= */
bool buscar_chave(ArvoreBPlus *arv, const void *chave, long *endereco_retorno) {
    if (arv->endereco_raiz == ENDERECO_NULO) return false;
    
    long end_atual = arv->endereco_raiz;
    Pagina pag;
    
    while (true) {
        carregar_pagina_disco(arv, end_atual, &pag);
        
        if (pag.eh_folha) {
            // Procura na folha
            for (int i = 0; i < pag.qtd_elementos; i++) {
                void *chave_deserializada = arv->deserializar(pag.chaves[i]);
                int resultado = arv->comparar(chave, chave_deserializada);
                arv->liberar(chave_deserializada);
                
                if (resultado == 0) {
                    if (endereco_retorno) *endereco_retorno = pag.registros_dados[i];
                    return true;
                }
            }
            return false; // Não encontrou
        }
        
        // Se for nó interno, decide qual filho descer
        int i = 0;
        while (i < pag.qtd_elementos) {
            void *chave_deserializada = arv->deserializar(pag.chaves[i]);
            int resultado = arv->comparar(chave, chave_deserializada);
            arv->liberar(chave_deserializada);
            
            if (resultado < 0) break;
            i++;
        }
        end_atual = pag.filhos[i];
    }
}

/* Structural helper type for split propagation */

/* ============================================================================
 * GERENCIAMENTO INTERNO DE ESPAÇO EM DISCO
 * Organiza o crescimento linear das páginas no arquivo.
 * ========================================================================= */
static long alocar_pagina(ArvoreBPlus *arv) {
    long endereco = arv->proximo_bloco_livre;
    arv->proximo_bloco_livre += arv->tamanho_no_bytes;
    return endereco;
}

/* ============================================================================
 * FUNÇÕES AUXILIARES DE NAVEGAÇÃO E INSERÇÃO
 * ========================================================================= */

// Desce da raiz até a página folha correspondente, guardando o caminho percorrido
static long descer_para_folha(ArvoreBPlus *arv, const void *chave, 
                               long *pilha_enderecos, int *pilha_indices, int *altura) {
    long end_atual = arv->endereco_raiz;
    *altura = 0;
    
    while (true) {
        Pagina pag;
        carregar_pagina_disco(arv, end_atual, &pag);
        if (pag.eh_folha) {
            return end_atual;
        }
        
        int i = 0;
        while (i < pag.qtd_elementos) {
            void *k = arv->deserializar(pag.chaves[i]);
            int cmp = arv->comparar(chave, k);
            arv->liberar(k);
            if (cmp < 0) break;
            i++;
        }
        
        if (pilha_enderecos) pilha_enderecos[*altura] = end_atual;
        if (pilha_indices)   pilha_indices[*altura] = i;
        (*altura)++;
        end_atual = pag.filhos[i];
    }
}

static void inserir_em_folha_ordenado(ArvoreBPlus *arv, Pagina *folha, const void *chave, long endereco_registro) {
    int i = folha->qtd_elementos - 1;
    unsigned char buf_chave[TAM_MAX_CHAVE];
    memset(buf_chave, 0, TAM_MAX_CHAVE);
    arv->serializar(chave, buf_chave);
    
    while (i >= 0) {
        void *k = arv->deserializar(folha->chaves[i]);
        int cmp = arv->comparar(chave, k);
        arv->liberar(k);
        if (cmp > 0) break;
        
        memcpy(folha->chaves[i + 1], folha->chaves[i], TAM_MAX_CHAVE);
        folha->registros_dados[i + 1] = folha->registros_dados[i];
        i--;
    }
    memcpy(folha->chaves[i + 1], buf_chave, TAM_MAX_CHAVE);
    folha->registros_dados[i + 1] = endereco_registro;
    folha->qtd_elementos++;
}

static void inserir_em_interno_ordenado(ArvoreBPlus *arv, Pagina *interno, const void *chave, long filho_direito) {
    int i = interno->qtd_elementos - 1;
    unsigned char buf_chave[TAM_MAX_CHAVE];
    memset(buf_chave, 0, TAM_MAX_CHAVE);
    arv->serializar(chave, buf_chave);
    
    while (i >= 0) {
        void *k = arv->deserializar(interno->chaves[i]);
        int cmp = arv->comparar(chave, k);
        arv->liberar(k);
        if (cmp > 0) break;
        
        memcpy(interno->chaves[i + 1], interno->chaves[i], TAM_MAX_CHAVE);
        interno->filhos[i + 2] = interno->filhos[i + 1];
        i--;
    }
    memcpy(interno->chaves[i + 1], buf_chave, TAM_MAX_CHAVE);
    interno->filhos[i + 2] = filho_direito;
    interno->qtd_elementos++;
}

/* ============================================================================
 * OPERAÇÕES DE CISÃO (SPLIT)
 * Tratam o estouro de capacidade (Overflow) das páginas em disco.
 * ========================================================================= */
static ResultadoSplit split_folha(ArvoreBPlus *arv, Pagina *folha) {
    ResultadoSplit res;
    res.houve_split = true;
    
    int total = folha->qtd_elementos;
    int meio = (total + 1) / 2;
    
    Pagina direita;
    direita.endereco_proprio = alocar_pagina(arv);
    direita.eh_folha = true;
    direita.qtd_elementos = total - meio;
    memset(direita.chaves, 0, sizeof(direita.chaves));
    memset(direita.registros_dados, 0, sizeof(direita.registros_dados));
    
    for (int i = meio; i < total; i++) {
        memcpy(direita.chaves[i - meio], folha->chaves[i], TAM_MAX_CHAVE);
        direita.registros_dados[i - meio] = folha->registros_dados[i];
    }
    direita.proxima_folha = folha->proxima_folha;
    
    folha->qtd_elementos = meio;
    folha->proxima_folha = direita.endereco_proprio;
    
    salvar_pagina_disco(arv, folha);
    salvar_pagina_disco(arv, &direita);
    
    memcpy(res.chave_promovida, direita.chaves[0], TAM_MAX_CHAVE);
    res.novo_filho_direito = direita.endereco_proprio;
    return res;
}

static ResultadoSplit split_interno(ArvoreBPlus *arv, Pagina *interno) {
    ResultadoSplit res;
    res.houve_split = true;
    
    int total = interno->qtd_elementos;
    int meio = total / 2;
    
    memcpy(res.chave_promovida, interno->chaves[meio], TAM_MAX_CHAVE);
    
    Pagina direita;
    direita.endereco_proprio = alocar_pagina(arv);
    direita.eh_folha = false;
    direita.qtd_elementos = total - meio - 1;
    memset(direita.chaves, 0, sizeof(direita.chaves));
    memset(direita.filhos, 0, sizeof(direita.filhos));
    direita.proxima_folha = ENDERECO_NULO;
    
    for (int i = meio + 1; i < total; i++) {
        memcpy(direita.chaves[i - meio - 1], interno->chaves[i], TAM_MAX_CHAVE);
    }
    for (int i = meio + 1; i <= total; i++) {
        direita.filhos[i - meio - 1] = interno->filhos[i];
    }
    
    interno->qtd_elementos = meio;
    
    salvar_pagina_disco(arv, interno);
    salvar_pagina_disco(arv, &direita);
    
    res.novo_filho_direito = direita.endereco_proprio;
    return res;
}

static void criar_nova_raiz(ArvoreBPlus *arv, long filho_esquerdo, const unsigned char *chave_buf, long filho_direito) {
    Pagina nova_raiz;
    nova_raiz.endereco_proprio = alocar_pagina(arv);
    nova_raiz.eh_folha = false;
    nova_raiz.qtd_elementos = 1;
    memset(nova_raiz.chaves, 0, sizeof(nova_raiz.chaves));
    memcpy(nova_raiz.chaves[0], chave_buf, TAM_MAX_CHAVE);
    memset(nova_raiz.filhos, 0, sizeof(nova_raiz.filhos));
    nova_raiz.filhos[0] = filho_esquerdo;
    nova_raiz.filhos[1] = filho_direito;
    nova_raiz.proxima_folha = ENDERECO_NULO;
    
    salvar_pagina_disco(arv, &nova_raiz);
    arv->endereco_raiz = nova_raiz.endereco_proprio;
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL DE INSERÇÃO
 * ========================================================================= */
bool inserir_chave(ArvoreBPlus *arv, const void *chave, long endereco_registro_rh) {
    if (buscar_chave(arv, chave, NULL)) {
        return false; // Evita duplicações na árvore compostas (Nome + Nascimento)
    }
    
    if (arv->endereco_raiz == ENDERECO_NULO) {
        Pagina raiz;
        raiz.endereco_proprio = alocar_pagina(arv);
        raiz.eh_folha = true;
        raiz.qtd_elementos = 0;
        memset(raiz.chaves, 0, sizeof(raiz.chaves));
        memset(raiz.registros_dados, 0, sizeof(raiz.registros_dados));
        raiz.proxima_folha = ENDERECO_NULO;
        
        inserir_em_folha_ordenado(arv, &raiz, chave, endereco_registro_rh);
        salvar_pagina_disco(arv, &raiz);
        arv->endereco_raiz = raiz.endereco_proprio;
        atualizar_cabecalho(arv);
        return true;
    }
    
    long pilha_enderecos[64];
    int pilha_indices[64];
    int altura = 0;
    
    long folha_end = descer_para_folha(arv, chave, pilha_enderecos, pilha_indices, &altura);
    
    Pagina folha;
    carregar_pagina_disco(arv, folha_end, &folha);
    inserir_em_folha_ordenado(arv, &folha, chave, endereco_registro_rh);
    
    if (folha.qtd_elementos <= MAX_CHAVES) {
        salvar_pagina_disco(arv, &folha);
        return true;
    }
    
    ResultadoSplit res = split_folha(arv, &folha);
    int nivel = altura - 1;
    
    while (res.houve_split && nivel >= 0) {
        long pai_end = pilha_enderecos[nivel];
        Pagina pai;
        carregar_pagina_disco(arv, pai_end, &pai);
        
        void *k = arv->deserializar(res.chave_promovida);
        inserir_em_interno_ordenado(arv, &pai, k, res.novo_filho_direito);
        arv->liberar(k);
        
        if (pai.qtd_elementos <= MAX_CHAVES) {
            salvar_pagina_disco(arv, &pai);
            res.houve_split = false;
        } else {
            res = split_interno(arv, &pai);
        }
        nivel--;
    }
    
    if (res.houve_split) {
        long raiz_antiga = (altura == 0) ? folha_end : pilha_enderecos[0];
        criar_nova_raiz(arv, raiz_antiga, res.chave_promovida, res.novo_filho_direito);
        atualizar_cabecalho(arv);
    }
    
    return true;
}

/* ============================================================================
 * OPERAÇÕES DE REMOÇÃO FISCA (UNDERFLOW)
 * ========================================================================= */
static void remover_de_folha(ArvoreBPlus *arv, Pagina *folha, const void *chave) {
    int pos = -1;
    for (int i = 0; i < folha->qtd_elementos; i++) {
        void *k = arv->deserializar(folha->chaves[i]);
        int cmp = arv->comparar(chave, k);
        arv->liberar(k);
        if (cmp == 0) { pos = i; break; }
    }
    if (pos == -1) return;
    for (int i = pos; i < folha->qtd_elementos - 1; i++) {
        memcpy(folha->chaves[i], folha->chaves[i + 1], TAM_MAX_CHAVE);
        folha->registros_dados[i] = folha->registros_dados[i + 1];
    }
    folha->qtd_elementos--;
}

static void remover_de_interno(Pagina *interno, int pos) {
    for (int i = pos; i < interno->qtd_elementos - 1; i++) {
        memcpy(interno->chaves[i], interno->chaves[i + 1], TAM_MAX_CHAVE);
    }
    for (int i = pos + 1; i < interno->qtd_elementos; i++) {
        interno->filhos[i] = interno->filhos[i + 1];
    }
    interno->qtd_elementos--;
}

bool remover_chave(ArvoreBPlus *arv, const void *chave) {
    if (arv->endereco_raiz == ENDERECO_NULO) return false;
    if (!buscar_chave(arv, chave, NULL)) return false;

    long pilha_enderecos[64];
    int pilha_indices[64];
    int altura = 0;
    long folha_end = descer_para_folha(arv, chave, pilha_enderecos, pilha_indices, &altura);

    Pagina folha;
    carregar_pagina_disco(arv, folha_end, &folha);
    remover_de_folha(arv, &folha, chave);

    if (altura == 0) {
        if (folha.qtd_elementos == 0) {
            arv->endereco_raiz = ENDERECO_NULO;
        } else {
            salvar_pagina_disco(arv, &folha);
        }
        atualizar_cabecalho(arv);
        return true;
    }

    if (folha.qtd_elementos >= MIN_CHAVES) {
        salvar_pagina_disco(arv, &folha);
        return true;
    }

    Pagina atual = folha;
    int nivel = altura - 1;

    while (true) {
        long pai_end = pilha_enderecos[nivel];
        int idx_no_pai = pilha_indices[nivel];
        Pagina pai;
        carregar_pagina_disco(arv, pai_end, &pai);

        long irmao_esq_end = (idx_no_pai > 0) ? pai.filhos[idx_no_pai - 1] : ENDERECO_NULO;
        long irmao_dir_end = (idx_no_pai < pai.qtd_elementos) ? pai.filhos[idx_no_pai + 1] : ENDERECO_NULO;

        bool resolvido = false;

        // Redistribuição: Tenta pegar emprestado do irmão esquerdo
        if (!resolvido && irmao_esq_end != ENDERECO_NULO) {
            Pagina esq;
            carregar_pagina_disco(arv, irmao_esq_end, &esq);
            if (esq.qtd_elementos > MIN_CHAVES) {
                if (atual.eh_folha) {
                    for (int i = atual.qtd_elementos; i > 0; i--) {
                        memcpy(atual.chaves[i], atual.chaves[i - 1], TAM_MAX_CHAVE);
                        atual.registros_dados[i] = atual.registros_dados[i - 1];
                    }
                    memcpy(atual.chaves[0], esq.chaves[esq.qtd_elementos - 1], TAM_MAX_CHAVE);
                    atual.registros_dados[0] = esq.registros_dados[esq.qtd_elementos - 1];
                    atual.qtd_elementos++;
                    esq.qtd_elementos--;

                    memcpy(pai.chaves[idx_no_pai - 1], atual.chaves[0], TAM_MAX_CHAVE);
                } else {
                    for (int i = atual.qtd_elementos; i > 0; i--) {
                        memcpy(atual.chaves[i], atual.chaves[i - 1], TAM_MAX_CHAVE);
                    }
                    for (int i = atual.qtd_elementos + 1; i > 0; i--) {
                        atual.filhos[i] = atual.filhos[i - 1];
                    }
                    memcpy(atual.chaves[0], pai.chaves[idx_no_pai - 1], TAM_MAX_CHAVE);
                    atual.filhos[0] = esq.filhos[esq.qtd_elementos];
                    atual.qtd_elementos++;

                    memcpy(pai.chaves[idx_no_pai - 1], esq.chaves[esq.qtd_elementos - 1], TAM_MAX_CHAVE);
                    esq.qtd_elementos--;
                }
                salvar_pagina_disco(arv, &esq);
                salvar_pagina_disco(arv, &atual);
                salvar_pagina_disco(arv, &pai);
                resolvido = true;
            }
        }

        // Redistribuição: Tenta pegar emprestado do irmão direito
        if (!resolvido && irmao_dir_end != ENDERECO_NULO) {
            Pagina dir;
            carregar_pagina_disco(arv, irmao_dir_end, &dir);
            if (dir.qtd_elementos > MIN_CHAVES) {
                if (atual.eh_folha) {
                    memcpy(atual.chaves[atual.qtd_elementos], dir.chaves[0], TAM_MAX_CHAVE);
                    atual.registros_dados[atual.qtd_elementos] = dir.registros_dados[0];
                    atual.qtd_elementos++;

                    for (int i = 0; i < dir.qtd_elementos - 1; i++) {
                        memcpy(dir.chaves[i], dir.chaves[i + 1], TAM_MAX_CHAVE);
                        dir.registros_dados[i] = dir.registros_dados[i + 1];
                    }
                    dir.qtd_elementos--;

                    memcpy(pai.chaves[idx_no_pai], dir.chaves[0], TAM_MAX_CHAVE);
                } else {
                    memcpy(atual.chaves[atual.qtd_elementos], pai.chaves[idx_no_pai], TAM_MAX_CHAVE);
                    atual.filhos[atual.qtd_elementos + 1] = dir.filhos[0];
                    atual.qtd_elementos++;

                    memcpy(pai.chaves[idx_no_pai], dir.chaves[0], TAM_MAX_CHAVE);

                    for (int i = 0; i < dir.qtd_elementos - 1; i++) {
                        memcpy(dir.chaves[i], dir.chaves[i + 1], TAM_MAX_CHAVE);
                    }
                    for (int i = 0; i < dir.qtd_elementos; i++) {
                        dir.filhos[i] = dir.filhos[i + 1];
                    }
                    dir.qtd_elementos--;
                }
                salvar_pagina_disco(arv, &dir);
                salvar_pagina_disco(arv, &atual);
                salvar_pagina_disco(arv, &pai);
                resolvido = true;
            }
        }

        if (resolvido) return true;

        // FUSÃO (MERGE): Se não puder emprestar, junta as páginas irmãs
        if (irmao_esq_end != ENDERECO_NULO) {
            Pagina esq;
            carregar_pagina_disco(arv, irmao_esq_end, &esq);

            if (atual.eh_folha) {
                for (int i = 0; i < atual.qtd_elementos; i++) {
                    memcpy(esq.chaves[esq.qtd_elementos + i], atual.chaves[i], TAM_MAX_CHAVE);
                    esq.registros_dados[esq.qtd_elementos + i] = atual.registros_dados[i];
                }
                esq.qtd_elementos += atual.qtd_elementos;
                esq.proxima_folha = atual.proxima_folha;
            } else {
                memcpy(esq.chaves[esq.qtd_elementos], pai.chaves[idx_no_pai - 1], TAM_MAX_CHAVE);
                for (int i = 0; i < atual.qtd_elementos; i++) {
                    memcpy(esq.chaves[esq.qtd_elementos + 1 + i], atual.chaves[i], TAM_MAX_CHAVE);
                }
                for (int i = 0; i <= atual.qtd_elementos; i++) {
                    esq.filhos[esq.qtd_elementos + 1 + i] = atual.filhos[i];
                }
                esq.qtd_elementos += atual.qtd_elementos + 1;
            }
            salvar_pagina_disco(arv, &esq);
            remover_de_interno(&pai, idx_no_pai - 1);
        } else {
            Pagina dir;
            carregar_pagina_disco(arv, irmao_dir_end, &dir);

            if (atual.eh_folha) {
                for (int i = 0; i < dir.qtd_elementos; i++) {
                    memcpy(atual.chaves[atual.qtd_elementos + i], dir.chaves[i], TAM_MAX_CHAVE);
                    atual.registros_dados[atual.qtd_elementos + i] = dir.registros_dados[i];
                }
                atual.qtd_elementos += dir.qtd_elementos;
                atual.proxima_folha = dir.proxima_folha;
            } else {
                memcpy(atual.chaves[atual.qtd_elementos], pai.chaves[idx_no_pai], TAM_MAX_CHAVE);
                for (int i = 0; i < dir.qtd_elementos; i++) {
                    memcpy(atual.chaves[atual.qtd_elementos + 1 + i], dir.chaves[i], TAM_MAX_CHAVE);
                }
                for (int i = 0; i <= dir.qtd_elementos; i++) {
                    atual.filhos[atual.qtd_elementos + 1 + i] = dir.filhos[i];
                }
                atual.qtd_elementos += dir.qtd_elementos + 1;
            }
            salvar_pagina_disco(arv, &atual);
            remover_de_interno(&pai, idx_no_pai);
        }

        if (nivel == 0) {
            if (pai.qtd_elementos == 0) {
                arv->endereco_raiz = pai.filhos[0];
            } else {
                salvar_pagina_disco(arv, &pai);
            }
            atualizar_cabecalho(arv);
            return true;
        }

        if (pai.qtd_elementos >= MIN_CHAVES) {
            salvar_pagina_disco(arv, &pai);
            return true;
        }

        salvar_pagina_disco(arv, &pai);
        atual = pai;
        nivel--;
    }
}

/* ============================================================================
 * BUSCA POR INTERVALO (SELEÇÃO)
 * ========================================================================= */
void buscar_intervalo(ArvoreBPlus *arv, const void *chaveA, const void *chaveB, VisitarNoFn visitar, void *contexto) {
    if (arv->endereco_raiz == ENDERECO_NULO) return;
    
    int altura = 0;
    long folha_end = descer_para_folha(arv, chaveA, NULL, NULL, &altura);
    
    while (folha_end != ENDERECO_NULO) {
        Pagina folha;
        carregar_pagina_disco(arv, folha_end, &folha);
        
        for (int i = 0; i < folha.qtd_elementos; i++) {
            void *k = arv->deserializar(folha.chaves[i]);
            int cmpA = arv->comparar(k, chaveA);
            int cmpB = arv->comparar(k, chaveB);
            
            if (cmpA > 0 && cmpB < 0) {
                visitar(k, folha.registros_dados[i], contexto);
            }
            arv->liberar(k);
            
            if (cmpB >= 0) {
                return; // Encerra assim que passar do limite alfabético superior
            }
        }
        folha_end = folha.proxima_folha;
    }
}

/* ============================================================================
 * IMPRESSÃO TEXTUAL HIERÁRQUICA (EXIGÊNCIA DO RELATÓRIO)
 * ========================================================================= */
static void imprimir_no_recursivo(ArvoreBPlus *arv, long endereco, int nivel) {
    if (endereco == ENDERECO_NULO) return;
    
    Pagina pag;
    carregar_pagina_disco(arv, endereco, &pag);
    
    for (int t = 0; t < nivel; t++) printf("    ");
    printf("%s[End=%ld] (", pag.eh_folha ? "FOLHA " : "NO-INT", endereco);
    
    for (int i = 0; i < pag.qtd_elementos; i++) {
        void *k = arv->deserializar(pag.chaves[i]);
        if (i > 0) printf(" | ");
        arv->imprimir(k);
        arv->liberar(k);
    }
    printf(")\n");
    
    if (!pag.eh_folha) {
        for (int i = 0; i <= pag.qtd_elementos; i++) {
            imprimir_no_recursivo(arv, pag.filhos[i], nivel + 1);
        }
    }
}

void imprimir_estrutura_arvore(ArvoreBPlus *arv) {
    if (arv->endereco_raiz == ENDERECO_NULO) {
        printf("(Árvore vazia)\n");
        return;
    }
    printf("=== Estrutura da Árvore B+ (raiz no endereço %ld) ===\n", arv->endereco_raiz);
    imprimir_no_recursivo(arv, arv->endereco_raiz, 0);
    printf("=====================================================\n");
}