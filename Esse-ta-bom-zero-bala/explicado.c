#include "Bplus1.h"

// ============================================================================
// 1. INICIALIZAÇÃO E FECHAMENTO (GERENCIAMENTO DO ARQUIVO)
// ============================================================================

ArvoreBPlus* criar_arvore(const char *nome_arquivo, int ordem, size_t tamanho_chave, ComparaChaveFunc func_compara) {
    ArvoreBPlus *arvore = (ArvoreBPlus*) malloc(sizeof(ArvoreBPlus));
    arvore->compara = func_compara;

    /* * DIDÁTICA: O TAMANHO DA PÁGINA
     * Uma Árvore B+ em disco precisa que todas as "Páginas" (Nós) tenham o mesmo 
     * tamanho exato em bytes. Assim, podemos pular de uma para outra fazendo contas matemáticas.
     * * Por que (ordem + 1) e (ordem + 2)?
     * Para facilitar a inserção! Nós permitimos que a página "estoure" (overflow) temporariamente 
     * na memória RAM. Assim, inserimos o dado excedente, ordenamos e SÓ ENTÃO quebramos a 
     * página ao meio (cisalhamento/split).
     */
    size_t tam_cabecalho = sizeof(CabecalhoPagina);
    size_t tam_chaves = (ordem + 1) * tamanho_chave;
    size_t tam_filhos = (ordem + 2) * sizeof(int);
    
    arvore->cabecalho.tamanho_no = tam_cabecalho + tam_chaves + tam_filhos;
    arvore->cabecalho.tamanho_chave = tamanho_chave;
    arvore->cabecalho.ordem = ordem;

    // Tenta abrir arquivo existente (leitura e escrita em binário)
    arvore->arquivo_indice = fopen(nome_arquivo, "rb+");
    
    if (arvore->arquivo_indice == NULL) {
        // CUIDADO: Arquivo não existia. Precisamos criar (wb+) e inicializar o Cabeçalho Mestre.
        arvore->arquivo_indice = fopen(nome_arquivo, "wb+");
        
        arvore->cabecalho.qtdPaginas = 0;
        arvore->cabecalho.raiz = -1; // -1 significa que a árvore está vazia (NULL)
        arvore->cabecalho.qtdRegistros = 0;

        // Salva as configurações logo no byte 0 do disco
        fseek(arvore->arquivo_indice, 0, SEEK_SET);
        fwrite(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
    } else {
        // Árvore já existe! Carregamos as informações para saber onde está a raiz.
        fseek(arvore->arquivo_indice, 0, SEEK_SET);
        fread(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
    }
    
    return arvore;
}

void fechar_arvore(ArvoreBPlus *arvore) {
    if (arvore != NULL) {
        if (arvore->arquivo_indice != NULL) {
            // CRÍTICO: Se a raiz mudou de lugar ou páginas foram criadas, 
            // precisamos atualizar o cabeçalho mestre no disco antes de fechar!
            fseek(arvore->arquivo_indice, 0, SEEK_SET);
            fwrite(&arvore->cabecalho, sizeof(CabecalhoArvore), 1, arvore->arquivo_indice);
            fclose(arvore->arquivo_indice);
        }
        free(arvore);
    }
}

// ============================================================================
// 2. PONTE ENTRE RAM E DISCO RÍGIDO (I/O)
// ============================================================================

/*
 * DIDÁTICA: O que é PaginaRAM?
 * Ler o disco byte a byte é muito lento. Nós criamos uma "forma" na memória RAM (PaginaRAM),
 * copiamos o bloco inteiro do disco para ela de uma vez, manipulamos os dados (inserir, remover) 
 * e depois devolvemos o bloco inteiro para o disco.
 */
PaginaRAM alocar_pagina_ram(ArvoreBPlus *arvore) {
    PaginaRAM p;
    int ordem = arvore->cabecalho.ordem;
    
    // calloc zera a memória. Isso evita gravar "lixo" (dados antigos da RAM) no HD.
    p.chaves = calloc(ordem + 1, arvore->cabecalho.tamanho_chave);
    p.filhos = calloc(ordem + 2, sizeof(int));
    
    return p;
}

void liberar_pagina_ram(PaginaRAM *pag) {
    free(pag->chaves);
    free(pag->filhos);
}

void ler_pagina(ArvoreBPlus *arvore, int index, PaginaRAM *pag) {
    // FÓRMULA MÁGICA: Onde está a página de índice 'x'?
    // Pula o cabeçalho da árvore + (x vezes o tamanho de uma página)
    long offset = sizeof(CabecalhoArvore) + (index * arvore->cabecalho.tamanho_no);
    fseek(arvore->arquivo_indice, offset, SEEK_SET);

    // Despeja os bytes lidos do disco direto nas variáveis da RAM
    fread(&pag->cabecalho, sizeof(CabecalhoPagina), 1, arvore->arquivo_indice);
    fread(pag->chaves, arvore->cabecalho.tamanho_chave, arvore->cabecalho.ordem + 1, arvore->arquivo_indice);
    fread(pag->filhos, sizeof(int), arvore->cabecalho.ordem + 2, arvore->arquivo_indice);
}

void gravar_pagina(ArvoreBPlus *arvore, int index, PaginaRAM *pag) {
    // Mesma fórmula da leitura, mas agora para sobrescrever os dados no disco
    long offset = sizeof(CabecalhoArvore) + (index * arvore->cabecalho.tamanho_no);
    fseek(arvore->arquivo_indice, offset, SEEK_SET);

    fwrite(&pag->cabecalho, sizeof(CabecalhoPagina), 1, arvore->arquivo_indice);
    fwrite(pag->chaves, arvore->cabecalho.tamanho_chave, arvore->cabecalho.ordem + 1, arvore->arquivo_indice);
    fwrite(pag->filhos, sizeof(int), arvore->cabecalho.ordem + 2, arvore->arquivo_indice);
    
    // fflush avisa ao Sistema Operacional: "Pare de guardar no cache e escreva no HD físico agora!"
    fflush(arvore->arquivo_indice); 
}

// Procura buracos no arquivo (páginas que foram apagadas) para reaproveitar espaço.
int buscar_pagina_livre(ArvoreBPlus *arvore) {
    CabecalhoPagina cab;
    int k = 0;
    
    fseek(arvore->arquivo_indice, sizeof(CabecalhoArvore), SEEK_SET);

    // Lê apenas os cabeçalhos pulando o resto. É mais rápido!
    while (fread(&cab, sizeof(CabecalhoPagina), 1, arvore->arquivo_indice)) {
        if (cab.foiDeletada) return k; // Achou espaço reciclado!
        
        k++;
        // Pula as chaves e filhos para cair no cabeçalho da próxima página
        long bytes_a_pular = arvore->cabecalho.tamanho_no - sizeof(CabecalhoPagina);
        fseek(arvore->arquivo_indice, bytes_a_pular, SEEK_CUR);
    }
    return k; // Se não tem buracos, retorna o final do arquivo para criar uma página nova.
}

// ============================================================================
// 3. BUSCA (NAVEGANDO PELA ÁRVORE)
// ============================================================================

bool buscar_registro(ArvoreBPlus *arvore, void *chave, int *index_retorno) {
    if (arvore->cabecalho.raiz == -1) {
        *index_retorno = -1;
        return false; // Árvore vazia
    }

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    while (true) {
        ler_pagina(arvore, index_atual, &pag);

        if (pag.cabecalho.tipo == FOLHA) {
            // CHEGAMOS NA BASE DA ÁRVORE (Onde os dados reais ficam)
            *index_retorno = pag.cabecalho.index; 
            
            for (int i = 0; i < pag.cabecalho.qtdElementos; ++i) {
                void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
                
                // compara() retorna 0 se forem iguais
                if (arvore->compara(chave, chave_atual) == 0) {
                    *index_retorno = pag.filhos[i]; // Retorna a posição do registro no arquivo de dados
                    liberar_pagina_ram(&pag);
                    return true;
                }
            }
            liberar_pagina_ram(&pag);
            return false; // Chave não existe
        } 
        else {
            // NÓ INTERNO (Placas de sinalização)
            // Precisamos descobrir em qual porta (filho) descer.
            int i;
            for (i = 0; i < pag.cabecalho.qtdElementos; ++i) {
                void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
                int comparacao = arvore->compara(chave, chave_atual);
                
                if (comparacao < 0) {
                    index_atual = pag.filhos[i]; // Menor? Desce pela porta da Esquerda
                    break;
                } 
                else if (comparacao == 0) {
                    index_atual = pag.filhos[i + 1]; // Igual? Desce pela porta da Direita
                    break;
                }
            }
            // Se for maior que todos, desce pela última porta à direita
            if (i == pag.cabecalho.qtdElementos) {
                index_atual = pag.filhos[pag.cabecalho.qtdElementos];
            }
        }
    }
}

// ============================================================================
// 4. INSERÇÃO E OVERFLOW (DIVISÃO DE NÓS)
// ============================================================================

// Função auxiliar que apenas insere o dado no array mantendo a ordem alfabética/numérica.
void inserir_e_ordenar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, void *nova_chave, int novo_filho) {
    int i = pag->cabecalho.qtdElementos - 1;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    // Empurra todo mundo que é MAIOR para a direita, abrindo espaço no meio do array
    while (i >= 0) {
        void *chave_atual = (char*)pag->chaves + (i * tam_chave);
        
        if (arvore->compara(chave_atual, nova_chave) > 0) {
            memcpy((char*)pag->chaves + ((i + 1) * tam_chave), chave_atual, tam_chave);
            
            // Os filhos/ponteiros também andam junto com suas respectivas chaves
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
    
    // Insere no espaço que foi aberto
    memcpy((char*)pag->chaves + ((i + 1) * tam_chave), nova_chave, tam_chave);
    
    if (pag->cabecalho.tipo == FOLHA) {
        pag->filhos[i + 1] = novo_filho;
    } else {
        pag->filhos[i + 2] = novo_filho;
    }
    
    pag->cabecalho.qtdElementos++;
}

void fix_overflow(ArvoreBPlus *arvore, PaginaRAM *pagina) {
    /*
     * DIDÁTICA: O SPLIT (CISALHAMENTO)
     * Quando um nó ultrapassa a 'ordem', ele é dividido em dois.
     * PASSO 1: Criar a nova página (Irmã direita).
     * PASSO 2: Mover a metade direita dos elementos para a Irmã.
     * PASSO 3: Pegar o elemento do meio e "promover" (subir) para o nó Pai.
     */
    
    // PASSO 1: Nova página
    PaginaRAM nova_pagina = alocar_pagina_ram(arvore);
    int index_nova_pagina = buscar_pagina_livre(arvore);
    
    nova_pagina.cabecalho.tipo = pagina->cabecalho.tipo;
    nova_pagina.cabecalho.index = index_nova_pagina;
    nova_pagina.cabecalho.pai = pagina->cabecalho.pai;
    nova_pagina.cabecalho.foiDeletada = 0;
    
    int meio = pagina->cabecalho.qtdElementos / 2;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;
    
    // Mantém a conexão entre as folhas (Lista Encadeada para buscas por intervalo)
    nova_pagina.cabecalho.indexProximaPagina = pagina->cabecalho.indexProximaPagina;
    pagina->cabecalho.indexProximaPagina = index_nova_pagina;

    // PASSO 2: Transferência de metade dos dados
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

    // PASSO 3: Isolar a chave que vai subir para o Pai
    void *chave_promovida = malloc(tam_chave);
    memcpy(chave_promovida, nova_pagina.chaves, tam_chave);

    // REGRA DE OURO DA ÁRVORE B+:
    // Se dividirmos uma FOLHA, a chave promovida SOBE, mas uma cópia FICA na folha nova.
    // Se dividirmos uma INTERNA, a chave promovida SOBE e SOME do nó de baixo.
    if (nova_pagina.cabecalho.tipo == INTERNA) {
        // Apaga a chave promovida da página nova, deslocando todos para a esquerda
        for (int i = 0; i < nova_pagina.cabecalho.qtdElementos - 1; i++) {
            memcpy((char*)nova_pagina.chaves + (i * tam_chave), 
                   (char*)nova_pagina.chaves + ((i + 1) * tam_chave), tam_chave);
            nova_pagina.filhos[i] = nova_pagina.filhos[i + 1];
        }
        nova_pagina.filhos[nova_pagina.cabecalho.qtdElementos - 1] = nova_pagina.filhos[nova_pagina.cabecalho.qtdElementos];
        nova_pagina.cabecalho.qtdElementos--;

        // Como movemos nós filhos para a nova página, precisamos avisar a eles quem é o novo Pai
        for (int k = 0; k <= nova_pagina.cabecalho.qtdElementos; k++) {
            PaginaRAM temp_filho = alocar_pagina_ram(arvore);
            ler_pagina(arvore, nova_pagina.filhos[k], &temp_filho);
            temp_filho.cabecalho.pai = nova_pagina.cabecalho.index; // Novo pai!
            gravar_pagina(arvore, nova_pagina.filhos[k], &temp_filho);
            liberar_pagina_ram(&temp_filho);
        }
    }

    // Salva a quebra no disco
    gravar_pagina(arvore, pagina->cabecalho.index, pagina);
    gravar_pagina(arvore, nova_pagina.cabecalho.index, &nova_pagina);

    // PASSO 4: Inserir a chave promovida no PAI
    if (pagina->cabecalho.index == arvore->cabecalho.raiz) {
        // A raiz estourou! Precisamos de um nível novo no topo.
        PaginaRAM nova_raiz = alocar_pagina_ram(arvore);
        int index_raiz = buscar_pagina_livre(arvore);
        
        nova_raiz.cabecalho.tipo = INTERNA;
        nova_raiz.cabecalho.index = index_raiz;
        nova_raiz.cabecalho.pai = -1;
        nova_raiz.cabecalho.foiDeletada = 0;
        nova_raiz.cabecalho.qtdElementos = 1;
        
        memcpy(nova_raiz.chaves, chave_promovida, tam_chave);
        nova_raiz.filhos[0] = pagina->cabecalho.index;       // Esquerda
        nova_raiz.filhos[1] = nova_pagina.cabecalho.index;   // Direita
        
        gravar_pagina(arvore, index_raiz, &nova_raiz);
        
        // Atualiza os ponteiros das filhas e da árvore
        pagina->cabecalho.pai = index_raiz;
        nova_pagina.cabecalho.pai = index_raiz;
        gravar_pagina(arvore, pagina->cabecalho.index, pagina);
        gravar_pagina(arvore, nova_pagina.cabecalho.index, &nova_pagina);
        
        arvore->cabecalho.raiz = index_raiz;
        liberar_pagina_ram(&nova_raiz);
    } else {
        // Já existe um pai. Inserimos a chave promovida nele.
        PaginaRAM pai = alocar_pagina_ram(arvore);
        ler_pagina(arvore, pagina->cabecalho.pai, &pai);
        
        inserir_e_ordenar_ram(arvore, &pai, chave_promovida, nova_pagina.cabecalho.index);
        
        // EFEITO DOMINÓ (Propagação recursiva): E se o pai também estourar agora?
        if (pai.cabecalho.qtdElementos > arvore->cabecalho.ordem) {
            fix_overflow(arvore, &pai); // Chama a si mesmo!
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
        // Caso Especial: Primeira inserção da árvore
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

    // PASSO 1: Descer pela árvore até achar a folha correta
    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);
    
    while (true) {
        ler_pagina(arvore, index_atual, &pag);
        if (pag.cabecalho.tipo == FOLHA) break; // Chegou!
        
        int i;
        for (i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);
            if (arvore->compara(chave, chave_atual) < 0) break;
        }
        index_atual = pag.filhos[i];
    }

    // PASSO 2: Insere na folha
    inserir_e_ordenar_ram(arvore, &pag, chave, index_registro);
    
    // PASSO 3: Verifica se quebrou a regra (Overflow)
    if (pag.cabecalho.qtdElementos > arvore->cabecalho.ordem) {
        fix_overflow(arvore, &pag);
    } else {
        gravar_pagina(arvore, pag.cabecalho.index, &pag);
    }
    
    liberar_pagina_ram(&pag);
    arvore->cabecalho.qtdRegistros++;
    return true;
}

// ============================================================================
// 5. REMOÇÃO E UNDERFLOW (EMPRÉSTIMO OU FUSÃO)
// ============================================================================

void remover_e_deslocar_ram(ArvoreBPlus *arvore, PaginaRAM *pag, int index_remocao) {
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    // Quando removemos do meio, apagamos o buraco puxando a galera da direita pra esquerda.
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
    /*
     * DIDÁTICA: O UNDERFLOW
     * Se uma página ficou com menos de (ordem/2) elementos, ela está "doente" (pobre).
     * SOLUÇÃO 1: Tentar emprestar um elemento do Irmão (Redistribuição).
     * SOLUÇÃO 2: Se o irmão também for pobre, os dois se fundem em um só (Merge).
     */
    if (pagina->cabecalho.pai == -1) return;

    PaginaRAM pai = alocar_pagina_ram(arvore);
    ler_pagina(arvore, pagina->cabecalho.pai, &pai);

    // Onde estou em relação ao meu pai?
    int pos_pai = 0;
    while (pos_pai <= pai.cabecalho.qtdElementos && pai.filhos[pos_pai] != pagina->cabecalho.index) {
        pos_pai++;
    }

    int min_elementos = arvore->cabecalho.ordem / 2;
    size_t tam_chave = arvore->cabecalho.tamanho_chave;

    PaginaRAM irmao = alocar_pagina_ram(arvore);
    bool resolveu = false;

    // === TENTATIVA 1: Emprestar do Irmão Esquerdo ===
    if (pos_pai > 0) {
        ler_pagina(arvore, pai.filhos[pos_pai - 1], &irmao);
        
        if (irmao.cabecalho.qtdElementos > min_elementos) { // O irmão é "rico"! Pode emprestar.
            // Arreda todos da página atual pra direita pra abrir vaga no índice 0
            for (int i = pagina->cabecalho.qtdElementos; i > 0; i--) {
                memcpy((char*)pagina->chaves + (i * tam_chave), (char*)pagina->chaves + ((i - 1) * tam_chave), tam_chave);
                pagina->filhos[i] = pagina->filhos[i - 1];
            }
            
            // Pega o ÚLTIMO elemento do irmão e coloca no PRIMEIRO da página atual
            int ult_irmao = irmao.cabecalho.qtdElementos - 1;
            memcpy(pagina->chaves, (char*)irmao.chaves + (ult_irmao * tam_chave), tam_chave);
            pagina->filhos[0] = irmao.filhos[ult_irmao];
            
            pagina->cabecalho.qtdElementos++;
            irmao.cabecalho.qtdElementos--;

            // A placa de sinalização (chave) no pai precisa ser atualizada!
            memcpy((char*)pai.chaves + ((pos_pai - 1) * tam_chave), pagina->chaves, tam_chave);

            gravar_pagina(arvore, pagina->cabecalho.index, pagina);
            gravar_pagina(arvore, irmao.cabecalho.index, &irmao);
            gravar_pagina(arvore, pai.cabecalho.index, &pai);
            resolveu = true;
        }
    }

    // === TENTATIVA 2: Fusão (Merge) com Irmão Esquerdo ===
    if (!resolveu && pos_pai > 0) { // O irmão esquerdo também é pobre.
        // Despejamos todos os itens da página atual no final do irmão esquerdo
        for (int i = 0; i < pagina->cabecalho.qtdElementos; i++) {
            memcpy((char*)irmao.chaves + ((irmao.cabecalho.qtdElementos + i) * tam_chave), 
                   (char*)pagina->chaves + (i * tam_chave), tam_chave);
            irmao.filhos[irmao.cabecalho.qtdElementos + i] = pagina->filhos[i];
        }
        
        irmao.cabecalho.qtdElementos += pagina->cabecalho.qtdElementos;
        irmao.cabecalho.indexProximaPagina = pagina->cabecalho.indexProximaPagina;

        // Ao juntar as duas páginas, uma placa separadora no pai se torna inútil. Deve ser removida!
        remover_e_deslocar_ram(arvore, &pai, pos_pai - 1);
        pai.filhos[pos_pai - 1] = irmao.cabecalho.index; // Corrige ponteiro pós-deslocamento

        // Marca a página atual como "lixo" no disco para o buscar_pagina_livre() achar depois
        pagina->cabecalho.foiDeletada = 1; 

        gravar_pagina(arvore, irmao.cabecalho.index, &irmao);
        gravar_pagina(arvore, pagina->cabecalho.index, pagina);
        gravar_pagina(arvore, pai.cabecalho.index, &pai);
        
        // EFEITO DOMINÓ: Removendo um item do pai, ele pode sofrer underflow também!
        if (pai.cabecalho.pai != -1 && pai.cabecalho.qtdElementos < min_elementos) {
            fix_underflow(arvore, &pai); // Propagação recursiva
        } else if (pai.cabecalho.pai == -1 && pai.cabecalho.qtdElementos == 0) {
            // Se a raiz fundiu seus únicos filhos, a raiz morre e o filho assume o topo!
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

    // Desce verticalmente até a folha alvo
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

    // Varre o array da folha procurando a chave específica
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
        return false; // Não existe para deletar
    }

    // Aplica a exclusão em RAM e Grava no Disco
    remover_e_deslocar_ram(arvore, &pag, pos_remocao);
    gravar_pagina(arvore, pag.cabecalho.index, &pag);

    // Verifica restrições
    int min_elementos = arvore->cabecalho.ordem / 2;
    if (pag.cabecalho.qtdElementos < min_elementos && pag.cabecalho.index != arvore->cabecalho.raiz) {
        fix_underflow(arvore, &pag); // Ficou pobre, pede socorro aos irmãos
    } 
    else if (pag.cabecalho.index == arvore->cabecalho.raiz && pag.cabecalho.qtdElementos == 0) {
        // Árvore secou completamente
        pag.cabecalho.foiDeletada = 1;
        gravar_pagina(arvore, pag.cabecalho.index, &pag);
        arvore->cabecalho.raiz = -1;
    }

    arvore->cabecalho.qtdRegistros--;
    liberar_pagina_ram(&pag);
    return true;
}

// ============================================================================
// 6. BUSCA POR INTERVALO E IMPRESSÃO
// ============================================================================

void buscar_intervalo(ArvoreBPlus *arvore, void *chave_inicio, void *chave_fim, ProcessaRegistroFunc processar) {
    if (arvore->cabecalho.raiz == -1) return;

    int index_atual = arvore->cabecalho.raiz;
    PaginaRAM pag = alocar_pagina_ram(arvore);

    // DESCIDA: Vai pelo nó interno até a primeira folha do intervalo
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

    // VARREDURA HORIZONTAL: Usa a lista encadeada das folhas para pular de página em página
    // sem precisar descer desde a raiz novamente. (O maior benefício da B+ em relação a B normal)
    bool passou_do_fim = false;
    
    while (index_atual != -1 && !passou_do_fim) {
        ler_pagina(arvore, index_atual, &pag);

        for (int i = 0; i < pag.cabecalho.qtdElementos; i++) {
            void *chave_atual = (char*)pag.chaves + (i * arvore->cabecalho.tamanho_chave);

            int comp_inicio = arvore->compara(chave_atual, chave_inicio);
            int comp_fim = arvore->compara(chave_atual, chave_fim);

            // Intervalo exclusivo ex: Entre 10 e 20, não inclui nem 10 nem 20.
            if (comp_inicio > 0 && comp_fim < 0) {
                processar(chave_atual, pag.filhos[i]);
            } 
            else if (comp_fim >= 0) {
                // As chaves estão ordenadas, se esbarramos num item maior que o limite, já era, acabou.
                passou_do_fim = true;
                break;
            }
        }
        index_atual = pag.cabecalho.indexProximaPagina; // Pula para a folha vizinha
    }

    liberar_pagina_ram(&pag);
}

void imprimir_arvore(ArvoreBPlus *arvore, ImprimeChaveFunc imprime_chave) {
    if (arvore->cabecalho.raiz == -1) {
        printf("A arvore esta vazia no disco.\n");
        return;
    }

    // Implementação clássica de "Busca em Largura" (BFS) usando fila
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
                imprime_chave(chave_atual); 
                if (j < pag.cabecalho.qtdElementos - 1) printf(" | ");
            }
            printf(" ]  ");

            // Enfileira os filhos para aparecerem no próximo loop (Nível de baixo)
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