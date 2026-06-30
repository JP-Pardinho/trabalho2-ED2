#include "Bplus1.h"

// =======================================================
// Inicialização e Fechamento
// =======================================================

ArvoreBPlus* criar_arvore(const char *nome_arquivo, int ordem, size_t tamanho_chave, ComparaChaveFunc func_compara) {
    ArvoreBPlus *arvore = (ArvoreBPlus*) malloc(sizeof(ArvoreBPlus));
    arvore->compara = func_compara;

    // 1. Calcula o tamanho exato de uma página genérica no disco.
    // Usamos (ordem + 1) e (ordem + 2) para manter a folga de overflow do seu código antigo!
    size_t tam_cabecalho = sizeof(CabecalhoPagina);
    size_t tam_chaves = (ordem + 1) * tamanho_chave;
    size_t tam_filhos = (ordem + 2) * sizeof(int);
    
    arvore->cabecalho.tamanho_no = tam_cabecalho + tam_chaves + tam_filhos;
    arvore->cabecalho.tamanho_chave = tamanho_chave;
    arvore->cabecalho.ordem = ordem;

    // 2. Tenta abrir o arquivo existente
    arvore->arquivo_indice = fopen(nome_arquivo, "rb+");
    
    if (arvore->arquivo_indice == NULL) {
        // Arquivo não existe, cria do zero
        arvore->arquivo_indice = fopen(nome_arquivo, "wb+");
        
        arvore->cabecalho.qtdPaginas = 0;
        arvore->cabecalho.raiz = -1;
        arvore->cabecalho.qtdRegistros = 0;

        // Grava o cabeçalho mestre no byte 0
        fseek(arvore->arquivo_indice, 0, SEEK_SET);
        fwrite(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
    } else {
        // Arquivo já existe, carrega o cabeçalho mestre
        fseek(arvore->arquivo_indice, 0, SEEK_SET);
        fread(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
    }
    
    return arvore;
}

void fechar_arvore(ArvoreBPlus *arvore) {
    if (arvore != NULL) {
        if (arvore->arquivo_indice != NULL) {
            // Atualiza os metadados (como a quantidade de páginas e nova raiz) no disco antes de fechar
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

// Aloca a "casca" da página na memória para podermos manipular os dados lidos do HD
PaginaRAM alocar_pagina_ram(ArvoreBPlus *arvore) {
    PaginaRAM p;
    int ordem = arvore->cabecalho.ordem;
    
    // calloc zera a memória, evitando lixo na hora de gravar
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
    // A sua velha e confiável fórmula matemática de offsets!
    long offset = sizeof(CabecalhoArvore) + (index * arvore->cabecalho.tamanho_no);
    fseek(arvore->arquivo_indice, offset, SEEK_SET);

    // Lê as peças separadamente em blocos de bytes
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
    
    fflush(arvore->arquivo_indice); // Força a gravação imediata no HD
}

// =======================================================
// Gerenciamento de Espaço Livre no Disco
// =======================================================
int buscar_pagina_livre(ArvoreBPlus *arvore) {
    CabecalhoPagina cab;
    int k = 0;
    
    // Posiciona o leitor logo após o cabeçalho mestre da árvore
    fseek(arvore->arquivo_indice, sizeof(CabecalhoArvore), SEEK_SET);

    // Lê apenas os metadados (cabeçalho) de cada página física
    while (fread(&cab, sizeof(CabecalhoPagina), 1, arvore->arquivo_indice)) {
        if (cab.foiDeletada) {
            return k; // Achou um "buraco" livre no arquivo!
        }
        k++;
        
        // Pula os blocos de chaves e filhos no HD para cair direto no cabeçalho da próxima página
        long bytes_a_pular = arvore->cabecalho.tamanho_no - sizeof(CabecalhoPagina);
        fseek(arvore->arquivo_indice, bytes_a_pular, SEEK_CUR);
    }
    
    // Se não achou nenhuma página deletada, retorna o índice do final do arquivo
    return k; 
}

// =======================================================
// Operação de Busca por Igualdade
// =======================================================
bool buscar_registro(ArvoreBPlus *arvore, void *chave, int *index_retorno) {
    // Verificar se a árvore possui raiz
    if (arvore->cabecalho.raiz == -1) {
        *index_retorno = -1;
        return false;
    }

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    while (true) {
        // Carrega a página do disco para a RAM temporária
        ler_pagina(arvore, index_atual, &pag);

        if (pag.cabecalho.tipo == FOLHA) {
            *index_retorno = pag.cabecalho.index; // Guarda em qual folha a busca parou
            
            for (int i = 0; i < pag.cabecalho.qtdElementos; ++i) {
                // Encontra o endereço de memória exato da chave atual no array genérico
                void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
                
                // Usa o callback: Se for 0, as chaves são idênticas!
                if (arvore->compara(chave, chave_atual) == 0) {
                    *index_retorno = pag.filhos[i]; // Retorna o ID do registro (posição no arquivo de dados)
                    liberar_pagina_ram(&pag);
                    return true;
                }
            }

            // Chegou na folha certa, mas o elemento não existe
            liberar_pagina_ram(&pag);
            return false;
        } 
        else {
            // NÓ INTERNO: Descobre o caminho para descer na árvore
            int i;
            for (i = 0; i < pag.cabecalho.qtdElementos; ++i) {
                void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
                
                int comparacao = arvore->compara(chave, chave_atual);
                
                if (comparacao < 0) {
                    // A chave buscada é menor, desce pelo filho à esquerda
                    index_atual = pag.filhos[i];
                    break;
                } 
                else if (comparacao == 0) {
                    // A chave buscada é igual, desce pelo filho à direita do separador
                    index_atual = pag.filhos[i + 1];
                    break;
                }
            }
            
            // Se a chave buscada for maior que todas, vai pelo último filho à direita
            if (i == pag.cabecalho.qtdElementos) {
                index_atual = pag.filhos[pag.cabecalho.qtdElementos];
            }
        }
    }
}

// =======================================================
// Função Interna: Inserir e Ordenar na RAM
// =======================================================
void inserir_e_ordenar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, void *nova_chave, int novo_filho) {
    int i = pag->cabecalho.qtdElementos - 1;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    // Desloca as chaves maiores para a direita
    while (i >= 0) {
        void *chave_atual = (char*)pag->chaves + (i * tam_chave);
        
        if (arvore->compara(chave_atual, nova_chave) > 0) {
            // Empurra a chave 1 posição para a direita
            memcpy((char*)pag->chaves + ((i + 1) * tam_chave), chave_atual, tam_chave);
            
            // O ponteiro (filho) acompanha o deslocamento
            if (pag->cabecalho.tipo == FOLHA) {
                pag->filhos[i + 1] = pag->filhos[i];
            } else {
                pag->filhos[i + 2] = pag->filhos[i + 1]; // Interna tem 1 filho a mais
            }
            i--;
        } else {
            break; // Achou o local correto!
        }
    }
    
    // Insere a nova chave e o filho
    memcpy((char*)pag->chaves + ((i + 1) * tam_chave), nova_chave, tam_chave);
    
    if (pag->cabecalho.tipo == FOLHA) {
        pag->filhos[i + 1] = novo_filho;
    } else {
        pag->filhos[i + 2] = novo_filho;
    }
    
    pag->cabecalho.qtdElementos++;
}

// =======================================================
// Tratamento de Overflow (Cisalhamento)
// =======================================================
void fix_overflow(ArvoreBPlus *arvore, PaginaRAM *pagina) {
    PaginaRAM nova_pagina = alocar_pagina_ram(arvore);
    int index_nova_pagina = buscar_pagina_livre(arvore);
    
    nova_pagina.cabecalho.tipo = pagina->cabecalho.tipo;
    nova_pagina.cabecalho.index = index_nova_pagina;
    nova_pagina.cabecalho.pai = pagina->cabecalho.pai;
    nova_pagina.cabecalho.foiDeletada = 0;
    
    int meio = pagina->cabecalho.qtdElementos / 2;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;
    
    // Mantém a lista encadeada das folhas (o seu código antigo fazia isso excelentemente)
    nova_pagina.cabecalho.indexProximaPagina = pagina->cabecalho.indexProximaPagina;
    pagina->cabecalho.indexProximaPagina = index_nova_pagina;

    // Move a metade direita para a nova página
    int elementos_nova = 0;
    for (int i = meio; i < pagina->cabecalho.qtdElementos; i++) {
        memcpy((char*)nova_pagina.chaves + (elementos_nova * tam_chave), 
               (char*)pagina->chaves + (i * tam_chave), tam_chave);
               
        nova_pagina.filhos[elementos_nova] = pagina->filhos[i];
        elementos_nova++;
    }
    // Se for interna, tem que copiar o último filho que sobrou à direita
    if (pagina->cabecalho.tipo == INTERNA) {
        nova_pagina.filhos[elementos_nova] = pagina->filhos[pagina->cabecalho.qtdElementos];
    }

    nova_pagina.cabecalho.qtdElementos = elementos_nova;
    pagina->cabecalho.qtdElementos = meio;

    // Isola a chave que será promovida para o pai
    void *chave_promovida = malloc(tam_chave);
    memcpy(chave_promovida, nova_pagina.chaves, tam_chave);

    // Regra da B+: Se for INTERNA, a chave sobe e desaparece do nó dividido
    if (nova_pagina.cabecalho.tipo == INTERNA) {
        for (int i = 0; i < nova_pagina.cabecalho.qtdElementos - 1; i++) {
            memcpy((char*)nova_pagina.chaves + (i * tam_chave), 
                   (char*)nova_pagina.chaves + ((i + 1) * tam_chave), tam_chave);
            nova_pagina.filhos[i] = nova_pagina.filhos[i + 1];
        }
        nova_pagina.filhos[nova_pagina.cabecalho.qtdElementos - 1] = nova_pagina.filhos[nova_pagina.cabecalho.qtdElementos];
        nova_pagina.cabecalho.qtdElementos--;

        // Atualiza os ponteiros de pai dos filhos que foram transferidos de casa
        for (int k = 0; k <= nova_pagina.cabecalho.qtdElementos; k++) {
            PaginaRAM temp_filho = alocar_pagina_ram(arvore);
            ler_pagina(arvore, nova_pagina.filhos[k], &temp_filho);
            temp_filho.cabecalho.pai = nova_pagina.cabecalho.index;
            gravar_pagina(arvore, nova_pagina.filhos[k], &temp_filho);
            liberar_pagina_ram(&temp_filho);
        }
    }

    // Grava as duas páginas recém-divididas no disco
    gravar_pagina(arvore, pagina->cabecalho.index, pagina);
    gravar_pagina(arvore, nova_pagina.cabecalho.index, &nova_pagina);

    // ================== SUBIDA PARA O PAI ==================
    if (pagina->cabecalho.index == arvore->cabecalho.raiz) {
        // Criação de Nova Raiz
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
        
        // Atualiza as folhas para apontarem para a nova raiz
        pagina->cabecalho.pai = index_raiz;
        nova_pagina.cabecalho.pai = index_raiz;
        gravar_pagina(arvore, pagina->cabecalho.index, pagina);
        gravar_pagina(arvore, nova_pagina.cabecalho.index, &nova_pagina);
        
        arvore->cabecalho.raiz = index_raiz;
        liberar_pagina_ram(&nova_raiz);
    } else {
        // Inserção no Pai Existente (Propagação Recursiva)
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

// =======================================================
// Interface Pública de Inserção
// =======================================================
bool inserir_registro(ArvoreBPlus *arvore, void *chave, int index_registro) {
    if (arvore->cabecalho.raiz == -1) {
        // Árvore totalmente vazia: Cria a primeira folha
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

    // Navega do topo até a folha alvo
    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);
    
    while (true) {
        ler_pagina(arvore, index_atual, &pag);
        
        if (pag.cabecalho.tipo == FOLHA) break; // Chegou no destino!
        
        int i;
        for (i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
            if (arvore->compara(chave, chave_atual) < 0) break;
        }
        index_atual = pag.filhos[i]; // Desce para o próximo nível
    }

    // Insere e aciona o overflow, se necessário
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
// Função Interna: Remover e Deslocar na RAM
// =======================================================
void remover_e_deslocar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, int index_remocao) {
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    // Arrasta chaves e dados da direita para a esquerda
    for (int i = index_remocao; i < pag->cabecalho.qtdElementos - 1; i++) {
        memcpy((char*)pag->chaves + (i * tam_chave), 
               (char*)pag->chaves + ((i + 1) * tam_chave), tam_chave);
        
        if (pag->cabecalho.tipo == FOLHA) {
            pag->filhos[i] = pag->filhos[i + 1];
        } else {
            // Nó interno tem um filho a mais deslocado
            pag->filhos[i + 1] = pag->filhos[i + 2];
        }
    }
    pag->cabecalho.qtdElementos--;
}

// =======================================================
// Tratamento de Underflow (Redistribuição ou Fusão)
// =======================================================
void fix_underflow(ArvoreBPlus *arvore, PaginaRAM *pagina) {
    // Se for a raiz, o underflow é tratado de forma especial na função de remoção
    if (pagina->cabecalho.pai == -1) return;

    PaginaRAM pai = alocar_pagina_ram(arvore);
    ler_pagina(arvore, pagina->cabecalho.pai, &pai);

    // Encontra qual filho do pai é a nossa página atual
    int pos_pai = 0;
    while (pos_pai <= pai.cabecalho.qtdElementos && pai.filhos[pos_pai] != pagina->cabecalho.index) {
        pos_pai++;
    }

    int min_elementos = arvore->cabecalho.ordem / 2;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    PaginaRAM irmao = alocar_pagina_ram(arvore);
    bool resolveu = false;

    // ---------------------------------------------------------
    // TENTATIVA 1: Emprestar do Irmão Esquerdo
    // ---------------------------------------------------------
    if (pos_pai > 0) {
        ler_pagina(arvore, pai.filhos[pos_pai - 1], &irmao);
        
        if (irmao.cabecalho.qtdElementos > min_elementos) {
            // Abre espaço no início da página atual empurrando tudo pra direita
            for (int i = pagina->cabecalho.qtdElementos; i > 0; i--) {
                memcpy((char*)pagina->chaves + (i * tam_chave), (char*)pagina->chaves + ((i - 1) * tam_chave), tam_chave);
                pagina->filhos[i] = pagina->filhos[i - 1];
            }
            
            // Puxa o último elemento do irmão esquerdo para a primeira posição da página atual
            int ult_irmao = irmao.cabecalho.qtdElementos - 1;
            memcpy(pagina->chaves, (char*)irmao.chaves + (ult_irmao * tam_chave), tam_chave);
            pagina->filhos[0] = irmao.filhos[ult_irmao];
            
            pagina->cabecalho.qtdElementos++;
            irmao.cabecalho.qtdElementos--;

            // Atualiza a chave separadora no pai
            memcpy((char*)pai.chaves + ((pos_pai - 1) * tam_chave), pagina->chaves, tam_chave);

            gravar_pagina(arvore, pagina->cabecalho.index, pagina);
            gravar_pagina(arvore, irmao.cabecalho.index, &irmao);
            gravar_pagina(arvore, pai.cabecalho.index, &pai);
            resolveu = true;
        }
    }

    // ---------------------------------------------------------
    // TENTATIVA 2: Fundir (Merge) com o Irmão Esquerdo
    // ---------------------------------------------------------
    if (!resolveu && pos_pai > 0) {
        // Despeja todo o conteúdo da página atual no final do irmão esquerdo
        for (int i = 0; i < pagina->cabecalho.qtdElementos; i++) {
            memcpy((char*)irmao.chaves + ((irmao.cabecalho.qtdElementos + i) * tam_chave), 
                   (char*)pagina->chaves + (i * tam_chave), tam_chave);
            irmao.filhos[irmao.cabecalho.qtdElementos + i] = pagina->filhos[i];
        }
        
        irmao.cabecalho.qtdElementos += pagina->cabecalho.qtdElementos;
        irmao.cabecalho.indexProximaPagina = pagina->cabecalho.indexProximaPagina;

        // Remove a chave separadora e o ponteiro do pai
        remover_e_deslocar_ram(arvore, &pai, pos_pai - 1);
        // Corrige o ponteiro que a função acima arrastou incorretamente para o merge
        pai.filhos[pos_pai - 1] = irmao.cabecalho.index;

        pagina->cabecalho.foiDeletada = 1;

        gravar_pagina(arvore, irmao.cabecalho.index, &irmao);
        gravar_pagina(arvore, pagina->cabecalho.index, pagina);
        gravar_pagina(arvore, pai.cabecalho.index, &pai);
        
        // PROPAGAÇÃO RECURSIVA: Se o pai ficou pequeno, conserta ele!
        if (pai.cabecalho.pai != -1 && pai.cabecalho.qtdElementos < min_elementos) {
            fix_underflow(arvore, &pai);
        } else if (pai.cabecalho.pai == -1 && pai.cabecalho.qtdElementos == 0) {
            // A raiz esvaziou! O irmão assume como nova raiz da árvore.
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

// =======================================================
// Interface Pública de Remoção
// =======================================================
bool remover_registro(ArvoreBPlus *arvore, void *chave) {
    if (arvore->cabecalho.raiz == -1) return false;

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    // 1. Navega até a folha correta
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

    // 2. Procura a chave exata dentro da folha
    int pos_remocao = -1;
    for (int i = 0; i < pag.cabecalho.qtdElementos; i++) {
        void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
        if (arvore->compara(chave, chave_atual) == 0) {
            pos_remocao = i;
            break;
        }
    }

    // Se não encontrou, aborta
    if (pos_remocao == -1) {
        liberar_pagina_ram(&pag);
        return false;
    }

    // 3. Remove arrastando a memória
    remover_e_deslocar_ram(arvore, &pag, pos_remocao);
    
    // 4. Salva a folha atualizada no disco
    gravar_pagina(arvore, pag.cabecalho.index, &pag);

    // 5. Gatilho de Underflow
    int min_elementos = arvore->cabecalho.ordem / 2;
    if (pag.cabecalho.qtdElementos < min_elementos && pag.cabecalho.index != arvore->cabecalho.raiz) {
        fix_underflow(arvore, &pag);
    } 
    // Se a raiz ficou completamente vazia, marca a árvore como vazia
    else if (pag.cabecalho.index == arvore->cabecalho.raiz && pag.cabecalho.qtdElementos == 0) {
        pag.cabecalho.foiDeletada = 1;
        gravar_pagina(arvore, pag.cabecalho.index, &pag);
        arvore->cabecalho.raiz = -1;
    }

    arvore->cabecalho.qtdRegistros--;
    liberar_pagina_ram(&pag);
    return true;
}

// =======================================================
// Busca por Intervalo (Lista Ligada de Folhas)
// =======================================================
void buscar_intervalo(ArvoreBPlus *arvore, void *chave_inicio, void *chave_fim, ProcessaRegistroFunc processar) {
    if (arvore->cabecalho.raiz == -1) return;

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    // 1. Descida vertical até encontrar a folha do limite inferior
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

    // 2. Navegação horizontal (varrimento pelas folhas)
    bool passou_do_fim = false;
    
    while (index_atual != -1 && !passou_do_fim) {
        ler_pagina(arvore, index_atual, &pag);

        for (int i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);

            int comp_inicio = arvore->compara(chave_atual, chave_inicio);
            int comp_fim = arvore->compara(chave_atual, chave_fim);

            // O requisito pede intervalo ABERTO (Nome A, Nome B)
            if (comp_inicio > 0 && comp_fim < 0) {
                // Envia a chave e o offset do ficheiro de dados para o main.c processar
                processar(chave_atual, pag.filhos[i]);
            } 
            else if (comp_fim >= 0) {
                // Como as chaves estão ordenadas, podemos parar imediatamente
                passou_do_fim = true;
                break;
            }
        }
        
        // Salta para o próximo bloco físico no disco
        index_atual = pag.cabecalho.indexProximaPagina;
    }

    liberar_pagina_ram(&pag);
}

// =======================================================
// Impressão Hierárquica da Árvore B+ em Disco
// =======================================================
void imprimir_arvore(ArvoreBPlus *arvore, ImprimeChaveFunc imprime_chave) {
    if (arvore->cabecalho.raiz == -1) {
        printf("A arvore esta vazia no disco.\n");
        return;
    }

    // Fila simples para armazenar os índices das páginas de cada nível
    int fila[1000]; 
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
                
                // O callback injetado decide como fazer o printf desta chave
                imprime_chave(chave_atual); 
                
                if (j < pag.cabecalho.qtdElementos - 1) printf(" | ");
            }
            printf(" ]  ");

            // Se for nó interno, coloca os filhos na fila para o próximo nível
            if (pag.cabecalho.tipo == INTERNA) {
                for (int j = 0; j <= pag.cabecalho.qtdElementos; j++) {
                    if (pag.filhos[j] != -1) {
                        fila[fim++] = pag.filhos[j];
                    }
                }
            }
        }
        printf("\n");
        nivel++;
    }

    liberar_pagina_ram(&pag);
}