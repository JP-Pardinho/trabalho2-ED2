#include "Bplus.h"

// =======================================================
// Inicialização e Fechamento
// =======================================================
ArvoreBPlus* criar_arvore(const char *nome_arquivo, int ordem, size_t tamanho_chave, ComparaChaveFunc func_compara) {
    ArvoreBPlus *arvore = (ArvoreBPlus*) malloc(sizeof(ArvoreBPlus));
    if (!arvore) return NULL;
    
    arvore->compara = func_compara;

    size_t tam_cabecalho = sizeof(CabecalhoPagina);
    size_t tam_chaves = (ordem + 1) * tamanho_chave;
    size_t tam_filhos = (ordem + 2) * sizeof(int);
    
    arvore->cabecalho.tamanho_no = tam_cabecalho + tam_chaves + tam_filhos;
    arvore->cabecalho.tamanho_chave = tamanho_chave;
    arvore->cabecalho.ordem = ordem;

    arvore->arquivo_indice = fopen(nome_arquivo, "rb+");
    
    if (arvore->arquivo_indice == NULL) {
        arvore->arquivo_indice = fopen(nome_arquivo, "wb+");
        arvore->cabecalho.qtdPaginas = 0;
        arvore->cabecalho.raiz = -1;
        arvore->cabecalho.qtdRegistros = 0;

        fseek(arvore->arquivo_indice, 0, SEEK_SET);
        fwrite(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
    } else {
        fseek(arvore->arquivo_indice, 0, SEEK_SET);
        fread(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
    }
    return arvore;
}

void fechar_arvore(ArvoreBPlus *arvore) {
    if (arvore != NULL) {
        if (arvore->arquivo_indice != NULL) {
            fseek(arvore->arquivo_indice, 0, SEEK_SET);
            fwrite(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
            fclose(arvore->arquivo_indice);
        }
        free(arvore);
    }
}

// =======================================================
// Gerenciamento de Memória RAM Temporária
// =======================================================
PaginaRAM alocar_pagina_ram(ArvoreBPlus *arvore) {
    PaginaRAM p;
    int ordem = arvore->cabecalho.ordem;
    p.chaves = calloc(ordem + 1, arvore->cabecalho.tamanho_chave);
    p.filhos = calloc(ordem + 2, sizeof(int));
    return p;
}

void liberar_pagina_ram(PaginaRAM *pag) {
    free(pag->chaves);
    free(pag->filhos);
}

// =======================================================
// Leitura e Escrita FÍSICA (A Mágica do Disco)
// =======================================================
void ler_pagina(ArvoreBPlus *arvore, int index, PaginaRAM *pag) {
    long offset = sizeof(CabecalhoArvore) + (index * arvore->cabecalho.tamanho_no);
    fseek(arvore->arquivo_indice, offset, SEEK_SET);
    fread(&pag->cabecalho, sizeof(CabecalhoPagina), 1, arvore->arquivo_indice);
    fread(pag->chaves, arvore->cabecalho.tamanho_chave, arvore->cabecalho.ordem + 1, arvore->arquivo_indice);
    fread(pag->filhos, sizeof(int), arvore->cabecalho.ordem + 2, arvore->arquivo_indice);
}

void gravar_pagina(ArvoreBPlus *arvore, int index, PaginaRAM *pag) {
    long offset = sizeof(CabecalhoArvore) + (index * arvore->cabecalho.tamanho_no);
    fseek(arvore->arquivo_indice, offset, SEEK_SET);
    fwrite(&pag->cabecalho, sizeof(CabecalhoPagina), 1, arvore->arquivo_indice);
    fwrite(pag->chaves, arvore->cabecalho.tamanho_chave, arvore->cabecalho.ordem + 1, arvore->arquivo_indice);
    fwrite(pag->filhos, sizeof(int), arvore->cabecalho.ordem + 2, arvore->arquivo_indice);
    fflush(arvore->arquivo_indice); 
}

int buscar_pagina_livre(ArvoreBPlus *arvore) {
    CabecalhoPagina cab;
    int k = 0;
    fseek(arvore->arquivo_indice, sizeof(CabecalhoArvore), SEEK_SET);

    while (fread(&cab, sizeof(CabecalhoPagina), 1, arvore->arquivo_indice)) {
        if (cab.foiDeletada) return k; 
        k++;
        long bytes_a_pular = arvore->cabecalho.tamanho_no - sizeof(CabecalhoPagina);
        fseek(arvore->arquivo_indice, bytes_a_pular, SEEK_CUR);
    }
    return k; 
}

// =======================================================
// Operação de Busca por Igualdade
// =======================================================
bool buscar_registro(ArvoreBPlus *arvore, void *chave, int *index_retorno) {
    if (arvore->cabecalho.raiz == -1) {
        *index_retorno = -1;
        return false;
    }

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    while (true) {
        ler_pagina(arvore, index_atual, &pag);

        if (pag.cabecalho.tipo == FOLHA) {
            *index_retorno = pag.cabecalho.index; 
            for (int i = 0; i < pag.cabecalho.qtdElementos; ++i) {
                void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
                if (arvore->compara(chave, chave_atual) == 0) {
                    *index_retorno = pag.filhos[i]; 
                    liberar_pagina_ram(&pag);
                    return true;
                }
            }
            liberar_pagina_ram(&pag);
            return false;
        } else {
            int i;
            for (i = 0; i < pag.cabecalho.qtdElementos; ++i) {
                void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
                int comparacao = arvore->compara(chave, chave_atual);
                if (comparacao < 0) {
                    index_atual = pag.filhos[i];
                    break;
                } else if (comparacao == 0) {
                    index_atual = pag.filhos[i + 1];
                    break;
                }
            }
            if (i == pag.cabecalho.qtdElementos) {
                index_atual = pag.filhos[pag.cabecalho.qtdElementos];
            }
        }
    }
}

// =======================================================
// Funções Internas de Inserção e Overflow
// =======================================================
void inserir_e_ordenar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, void *nova_chave, int novo_filho) {
    int i = pag->cabecalho.qtdElementos - 1;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    while (i >= 0) {
        void *chave_atual = (char*)pag->chaves + (i * tam_chave);
        if (arvore->compara(chave_atual, nova_chave) > 0) {
            memcpy((char*)pag->chaves + ((i + 1) * tam_chave), chave_atual, tam_chave);
            if (pag->cabecalho.tipo == FOLHA) {
                pag->filhos[i + 1] = pag->filhos[i];
            } else {
                pag->filhos[i + 2] = pag->filhos[i + 1]; 
            }
            i--;
        } else {
            break; 
        }
    }
    
    memcpy((char*)pag->chaves + ((i + 1) * tam_chave), nova_chave, tam_chave);
    if (pag->cabecalho.tipo == FOLHA) {
        pag->filhos[i + 1] = novo_filho;
    } else {
        pag->filhos[i + 2] = novo_filho;
    }
    pag->cabecalho.qtdElementos++;
}

void fix_overflow(ArvoreBPlus *arvore, PaginaRAM *pagina) {
    PaginaRAM nova_pagina = alocar_pagina_ram(arvore);
    int index_nova_pagina = buscar_pagina_livre(arvore);
    
    nova_pagina.cabecalho.tipo = pagina->cabecalho.tipo;
    nova_pagina.cabecalho.index = index_nova_pagina;
    nova_pagina.cabecalho.pai = pagina->cabecalho.pai;
    nova_pagina.cabecalho.foiDeletada = 0;
    
    int meio = pagina->cabecalho.qtdElementos / 2;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;
    
    nova_pagina.cabecalho.indexProximaPagina = pagina->cabecalho.indexProximaPagina;
    pagina->cabecalho.indexProximaPagina = index_nova_pagina;

    int elementos_nova = 0;
    for (int i = meio; i < pagina->cabecalho.qtdElementos; i++) {
        memcpy((char*)nova_pagina.chaves + (elementos_nova * tam_chave), 
               (char*)pagina->chaves + (i * tam_chave), tam_chave);
        nova_pagina.filhos[elementos_nova] = pagina->filhos[i];
        elementos_nova++;
    }
    
    if (pagina->cabecalho.tipo == INTERNA) {
        nova_pagina.filhos[elementos_nova] = pagina->filhos[pagina->cabecalho.qtdElementos];
    }

    nova_pagina.cabecalho.qtdElementos = elementos_nova;
    pagina->cabecalho.qtdElementos = meio;

    void *chave_promovida = malloc(tam_chave);
    memcpy(chave_promovida, nova_pagina.chaves, tam_chave);

    if (nova_pagina.cabecalho.tipo == INTERNA) {
        for (int i = 0; i < nova_pagina.cabecalho.qtdElementos - 1; i++) {
            memcpy((char*)nova_pagina.chaves + (i * tam_chave), 
                   (char*)nova_pagina.chaves + ((i + 1) * tam_chave), tam_chave);
            nova_pagina.filhos[i] = nova_pagina.filhos[i + 1];
        }
        nova_pagina.filhos[nova_pagina.cabecalho.qtdElementos - 1] = nova_pagina.filhos[nova_pagina.cabecalho.qtdElementos];
        nova_pagina.cabecalho.qtdElementos--;

        for (int k = 0; k <= nova_pagina.cabecalho.qtdElementos; k++) {
            PaginaRAM temp_filho = alocar_pagina_ram(arvore);
            ler_pagina(arvore, nova_pagina.filhos[k], &temp_filho);
            temp_filho.cabecalho.pai = nova_pagina.cabecalho.index;
            gravar_pagina(arvore, nova_pagina.filhos[k], &temp_filho);
            liberar_pagina_ram(&temp_filho);
        }
    }

    gravar_pagina(arvore, pagina->cabecalho.index, pagina);
    gravar_pagina(arvore, nova_pagina.cabecalho.index, &nova_pagina);

    if (pagina->cabecalho.index == arvore->cabecalho.raiz) {
        PaginaRAM nova_raiz = alocar_pagina_ram(arvore);
        int index_raiz = buscar_pagina_livre(arvore);
        
        nova_raiz.cabecalho.tipo = INTERNA;
        nova_raiz.cabecalho.index = index_raiz;
        nova_raiz.cabecalho.pai = -1;
        nova_raiz.cabecalho.foiDeletada = 0;
        nova_raiz.cabecalho.qtdElementos = 1;
        
        memcpy(nova_raiz.chaves, chave_promovida, tam_chave);
        nova_raiz.filhos[0] = pagina->cabecalho.index;
        nova_raiz.filhos[1] = nova_pagina.cabecalho.index;
        
        gravar_pagina(arvore, index_raiz, &nova_raiz);
        
        pagina->cabecalho.pai = index_raiz;
        nova_pagina.cabecalho.pai = index_raiz;
        gravar_pagina(arvore, pagina->cabecalho.index, pagina);
        gravar_pagina(arvore, nova_pagina.cabecalho.index, &nova_pagina);
        
        arvore->cabecalho.raiz = index_raiz;
        liberar_pagina_ram(&nova_raiz);
    } else {
        PaginaRAM pai = alocar_pagina_ram(arvore);
        ler_pagina(arvore, pagina->cabecalho.pai, &pai);
        
        inserir_e_ordenar_ram(arvore, &pai, chave_promovida, nova_pagina.cabecalho.index);
        
        if (pai.cabecalho.qtdElementos > arvore->cabecalho.ordem) {
            fix_overflow(arvore, &pai);
        } else {
            gravar_pagina(arvore, pai.cabecalho.index, &pai);
        }
        liberar_pagina_ram(&pai);
    }

    free(chave_promovida);
    liberar_pagina_ram(&nova_pagina);
}

bool inserir_registro(ArvoreBPlus *arvore, void *chave, int index_registro) {
    if (arvore->cabecalho.raiz == -1) {
        PaginaRAM raiz = alocar_pagina_ram(arvore);
        int index = buscar_pagina_livre(arvore);
        
        raiz.cabecalho.tipo = FOLHA;
        raiz.cabecalho.index = index;
        raiz.cabecalho.pai = -1;
        raiz.cabecalho.foiDeletada = 0;
        raiz.cabecalho.qtdElementos = 1;
        raiz.cabecalho.indexProximaPagina = -1;
        
        memcpy(raiz.chaves, chave, arvore->cabecalho.tamanho_chave);
        raiz.filhos[0] = index_registro;
        
        gravar_pagina(arvore, index, &raiz);
        arvore->cabecalho.raiz = index;
        arvore->cabecalho.qtdPaginas++; 
        arvore->cabecalho.qtdRegistros++;
        liberar_pagina_ram(&raiz);
        return true;
    }

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);
    
    while (true) {
        ler_pagina(arvore, index_atual, &pag);
        if (pag.cabecalho.tipo == FOLHA) break; 
        
        int i;
        for (i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
            if (arvore->compara(chave, chave_atual) < 0) break;
        }
        index_atual = pag.filhos[i]; 
    }

    inserir_e_ordenar_ram(arvore, &pag, chave, index_registro);
    
    if (pag.cabecalho.qtdElementos > arvore->cabecalho.ordem) {
        fix_overflow(arvore, &pag);
    } else {
        gravar_pagina(arvore, pag.cabecalho.index, &pag);
    }
    
    liberar_pagina_ram(&pag);
    arvore->cabecalho.qtdRegistros++;
    return true;
}

// =======================================================
// Funções Internas de Remoção e Underflow
// =======================================================
void remover_e_deslocar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, int index_remocao) {
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    for (int i = index_remocao; i < pag->cabecalho.qtdElementos - 1; i++) {
        memcpy((char*)pag->chaves + (i * tam_chave), 
               (char*)pag->chaves + ((i + 1) * tam_chave), tam_chave);
        
        if (pag->cabecalho.tipo == FOLHA) {
            pag->filhos[i] = pag->filhos[i + 1];
        } else {
            pag->filhos[i + 1] = pag->filhos[i + 2];
        }
    }
    pag->cabecalho.qtdElementos--;
}

void fix_underflow(ArvoreBPlus *arvore, PaginaRAM *pagina) {
    if (pagina->cabecalho.pai == -1) return;

    PaginaRAM pai = alocar_pagina_ram(arvore);
    ler_pagina(arvore, pagina->cabecalho.pai, &pai);

    int pos_pai = 0;
    while (pos_pai <= pai.cabecalho.qtdElementos && pai.filhos[pos_pai] != pagina->cabecalho.index) {
        pos_pai++;
    }

    int min_elementos = arvore->cabecalho.ordem / 2;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    PaginaRAM irmao = alocar_pagina_ram(arvore);
    bool resolveu = false;

    if (pos_pai > 0) {
        ler_pagina(arvore, pai.filhos[pos_pai - 1], &irmao);
        
        if (irmao.cabecalho.qtdElementos > min_elementos) {
            for (int i = pagina->cabecalho.qtdElementos; i > 0; i--) {
                memcpy((char*)pagina->chaves + (i * tam_chave), (char*)pagina->chaves + ((i - 1) * tam_chave), tam_chave);
                pagina->filhos[i] = pagina->filhos[i - 1];
            }
            
            int ult_irmao = irmao.cabecalho.qtdElementos - 1;
            memcpy(pagina->chaves, (char*)irmao.chaves + (ult_irmao * tam_chave), tam_chave);
            pagina->filhos[0] = irmao.filhos[ult_irmao];
            
            pagina->cabecalho.qtdElementos++;
            irmao.cabecalho.qtdElementos--;

            memcpy((char*)pai.chaves + ((pos_pai - 1) * tam_chave), pagina->chaves, tam_chave);

            gravar_pagina(arvore, pagina->cabecalho.index, pagina);
            gravar_pagina(arvore, irmao.cabecalho.index, &irmao);
            gravar_pagina(arvore, pai.cabecalho.index, &pai);
            resolveu = true;
        }
    }

    if (!resolveu && pos_pai > 0) {
        for (int i = 0; i < pagina->cabecalho.qtdElementos; i++) {
            memcpy((char*)irmao.chaves + ((irmao.cabecalho.qtdElementos + i) * tam_chave), 
                   (char*)pagina->chaves + (i * tam_chave), tam_chave);
            irmao.filhos[irmao.cabecalho.qtdElementos + i] = pagina->filhos[i];
        }
        
        irmao.cabecalho.qtdElementos += pagina->cabecalho.qtdElementos;
        irmao.cabecalho.indexProximaPagina = pagina->cabecalho.indexProximaPagina;

        remover_e_deslocar_ram(arvore, &pai, pos_pai - 1);
        pai.filhos[pos_pai - 1] = irmao.cabecalho.index;
        pagina->cabecalho.foiDeletada = 1;

        gravar_pagina(arvore, irmao.cabecalho.index, &irmao);
        gravar_pagina(arvore, pagina->cabecalho.index, pagina);
        gravar_pagina(arvore, pai.cabecalho.index, &pai);
        
        if (pai.cabecalho.pai != -1 && pai.cabecalho.qtdElementos < min_elementos) {
            fix_underflow(arvore, &pai);
        } else if (pai.cabecalho.pai == -1 && pai.cabecalho.qtdElementos == 0) {
            irmao.cabecalho.pai = -1;
            arvore->cabecalho.raiz = irmao.cabecalho.index;
            gravar_pagina(arvore, irmao.cabecalho.index, &irmao);
            pai.cabecalho.foiDeletada = 1;
            gravar_pagina(arvore, pai.cabecalho.index, &pai);
        }
    }

    liberar_pagina_ram(&pai);
    liberar_pagina_ram(&irmao);
}

bool remover_registro(ArvoreBPlus *arvore, void *chave) {
    if (arvore->cabecalho.raiz == -1) return false;

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    while (true) {
        ler_pagina(arvore, index_atual, &pag);
        if (pag.cabecalho.tipo == FOLHA) break;
        
        int i;
        for (i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
            if (arvore->compara(chave, chave_atual) <= 0) break; 
        }
        index_atual = pag.filhos[i];
    }

    int pos_remocao = -1;
    for (int i = 0; i < pag.cabecalho.qtdElementos; i++) {
        void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
        if (arvore->compara(chave, chave_atual) == 0) {
            pos_remocao = i;
            break;
        }
    }

    if (pos_remocao == -1) {
        liberar_pagina_ram(&pag);
        return false;
    }

    remover_e_deslocar_ram(arvore, &pag, pos_remocao);
    gravar_pagina(arvore, pag.cabecalho.index, &pag);

    int min_elementos = arvore->cabecalho.ordem / 2;
    if (pag.cabecalho.qtdElementos < min_elementos && pag.cabecalho.index != arvore->cabecalho.raiz) {
        fix_underflow(arvore, &pag);
    } else if (pag.cabecalho.index == arvore->cabecalho.raiz && pag.cabecalho.qtdElementos == 0) {
        pag.cabecalho.foiDeletada = 1;
        gravar_pagina(arvore, pag.cabecalho.index, &pag);
        arvore->cabecalho.raiz = -1;
    }

    arvore->cabecalho.qtdRegistros--;
    liberar_pagina_ram(&pag);
    return true;
}

// =======================================================
// Interface de Busca por Intervalo e Impressão
// =======================================================
void buscar_intervalo(ArvoreBPlus *arvore, void *chave_inicio, void *chave_fim, ProcessaRegistroFunc processar) {
    if (arvore->cabecalho.raiz == -1) return;

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    while (true) {
        ler_pagina(arvore, index_atual, &pag);
        if (pag.cabecalho.tipo == FOLHA) break;

        int i;
        for (i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
            if (arvore->compara(chave_inicio, chave_atual) < 0) break;
        }
        index_atual = pag.filhos[i];
    }

    bool passou_do_fim = false;
    while (index_atual != -1 && !passou_do_fim) {
        ler_pagina(arvore, index_atual, &pag);

        for (int i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
            int comp_inicio = arvore->compara(chave_atual, chave_inicio);
            int comp_fim = arvore->compara(chave_atual, chave_fim);

            if (comp_inicio > 0 && comp_fim < 0) {
                processar(chave_atual, pag.filhos[i]);
            } else if (comp_fim >= 0) {
                passou_do_fim = true;
                break;
            }
        }
        index_atual = pag.cabecalho.indexProximaPagina;
    }
    liberar_pagina_ram(&pag);
}

void imprimir_arvore(ArvoreBPlus *arvore, ImprimeChaveFunc imprime_chave) {
    if (arvore->cabecalho.raiz == -1) {
        printf("A arvore esta vazia no disco.\n");
        return;
    }

    int *fila = malloc(1000 * sizeof(int)); // Fila dinâmica segura
    int inicio = 0, fim = 0;
    
    fila[fim++] = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);
    int nivel = 0;

    while (inicio < fim) {
        int tamanho_nivel = fim - inicio;
        printf("Nivel %d: ", nivel);

        for (int i = 0; i < tamanho_nivel; i++) {
            int index_atual = fila[inicio++];
            ler_pagina(arvore, index_atual, &pag);

            printf("[ ");
            for (int j = 0; j < pag.cabecalho.qtdElementos; j++) {
                void *chave_atual = (char*)pag.chaves + (j * arvore->cabecalho.tamanho_chave);
                imprime_chave(chave_atual); 
                if (j < pag.cabecalho.qtdElementos - 1) printf(" | ");
            }
            printf(" ]  ");

            if (pag.cabecalho.tipo == INTERNA) {
                for (int j = 0; j <= pag.cabecalho.qtdElementos; j++) {
                    if (pag.filhos[j] != -1) fila[fim++] = pag.filhos[j];
                }
            }
        }
        printf("\n");
        nivel++;
    }
    liberar_pagina_ram(&pag);
    free(fila);
}