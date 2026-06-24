#include "Bplus.h"

#include <stdio.h>
#include <stdlib.h>


No* criarNo(int folha) {
    No* novo = (No*) malloc(sizeof(No));
    novo->folha = folha;
    novo->n = 0;
    novo->prox = NULL;

    for(int i = 0; i < P + 1; i++)
        novo->filhos[i] = NULL;
    return novo;
}

int validarArvore(No* raiz){
    return validarArvoreRec(raiz, 1);
}

int validarArvoreRec(No* no, int ehRaiz) {

    if (no == NULL)
        return 1;

    int max = no->folha ? PFOLHA : P - 1;
    int min = no->folha ? (PFOLHA + 1) / 2 : (P + 1) / 2 - 1;

    // Verifica máximo
    if (no->n > max)
        return 0;

    // Verifica mínimo (exceto raiz)
    if (!ehRaiz && no->n < min)
        return 0;

    // Verifica ordem crescente
    for (int i = 1; i < no->n; i++) {
        if (no->chaves[i] <= no->chaves[i - 1])
            return 0;
    }

    // Se interno, validar filhos
    if (!no->folha) {
        for (int i = 0; i <= no->n; i++) {
            if (no->filhos[i] == NULL)
                return 0;

            if (!validarArvoreRec(no->filhos[i], 0))
                return 0;
        }
    }

    return 1;
}

int encontrarPosicao(No* no, int chave) {
    int i = 0;

    while (i < no->n && chave >= no->chaves[i]){
        i++;
    }
    return i;
}

No* buscarFolha(No* raiz, int chave) {
    if (raiz == NULL)
        return NULL;

    if (raiz->folha)
        return raiz;

    int pos = encontrarPosicao(raiz, chave);

    return buscarFolha(raiz->filhos[pos], chave);
}

int buscar(No* raiz, int chave) {
    No* folha = buscarFolha(raiz, chave);

    if (folha == NULL)
        return 0;

    for (int i = 0; i < folha->n; i++) {
        if (folha->chaves[i] == chave)
            return 1;
    }

    return 0;
}

void buscarIntervalo(No* raiz, int inicio, int fim) {
    No* folha = buscarFolha(raiz, inicio);
    if (folha == NULL) {
        printf("Nenhuma chave encontrada no intervalo.\n");
        return;
    }

    No* atual = folha;
    int encontrou = 0;
    printf("Valores no intervalo [%d, %d]: ", inicio, fim);
    while (atual != NULL) {
        for (int i = 0; i < atual->n; i++) {
            if (atual->chaves[i] >= inicio && atual->chaves[i] <= fim) {
                printf("%d ", atual->chaves[i]);
                encontrou = 1;
            } else if (atual->chaves[i] > fim) {
                printf("\n");
                return;
            }
        }
        atual = atual->prox;
    }
    if (!encontrou) {
        printf("Nenhuma chave encontrada.");
    }
    printf("\n");
}

Resultado inserirEmFolha(No* folha, int chave) {

    Resultado res;
    res.promoveu = 0;
    res.novoNo = NULL;

    // Inserir ordenado
    int i = folha->n - 1;

    while (i >= 0 && folha->chaves[i] > chave) {
        folha->chaves[i + 1] = folha->chaves[i];
        i--;
    }

    folha->chaves[i + 1] = chave;
    folha->n++;

    // Verificar overflow
    if (folha->n <= PFOLHA){
        return res;
    }
    // SPLIT

    int meio = folha->n / 2;

    No* novaFolha = criarNo(1);

    // mover metade direita
    for (int j = meio; j < folha->n; j++) {
        novaFolha->chaves[j - meio] = folha->chaves[j];
        novaFolha->n++;
    }

    folha->n = meio;

    // ajustar encadeamento
    novaFolha->prox = folha->prox;
    folha->prox = novaFolha;

    // promover primeira chave da direita
    res.promoveu = 1;
    res.chavePromovida = novaFolha->chaves[0];
    res.novoNo = novaFolha;

    return res;
}

Resultado inserirRec(No* no, int chave) {

    if (no->folha) {
        return inserirEmFolha(no, chave);
    }

    Resultado resFilho;

    int pos = encontrarPosicao(no, chave);

    resFilho = inserirRec(no->filhos[pos], chave);

    if (!resFilho.promoveu)
        return resFilho;

    // Se chegou aqui, filho promoveu
    // Agora precisamos inserir chavePromovida no nó atual

    // (aqui entra lógica de inserção interna + possível split interno)
    return inserirEmInterno(no, resFilho.chavePromovida, resFilho.novoNo);
}

Resultado inserirEmInterno(No* no, int chave, No* novoFilho) {

    Resultado res;
    res.promoveu = 0;
    res.novoNo = NULL;

    // Inserir ordenado
    int i = no->n - 1;

    while (i >= 0 && no->chaves[i] > chave) {
        no->chaves[i + 1] = no->chaves[i];
        no->filhos[i + 2] = no->filhos[i + 1];
        i--;
    }

    no->chaves[i + 1] = chave;
    no->filhos[i + 2] = novoFilho;
    no->n++;

    // Verificar overflow
    if (no->n <= P - 1)
        return res;

    // SPLIT INTERNO

    int meio = no->n / 2;

    No* novoInterno = criarNo(0);

    // chave que será promovida
    res.chavePromovida = no->chaves[meio];

    // mover chaves da direita (exceto a promovida)
    for (int j = meio + 1; j < no->n; j++) {
        novoInterno->chaves[j - (meio + 1)] = no->chaves[j];
        novoInterno->n++;
    }

    // mover filhos correspondentes
    for (int j = meio + 1; j <= no->n; j++) {
        novoInterno->filhos[j - (meio + 1)] = no->filhos[j];
    }

    no->n = meio;

    res.promoveu = 1;
    res.novoNo = novoInterno;

    return res;
}


No* inserir(No* raiz, int chave) {

    if (raiz == NULL) {
        raiz = criarNo(1);
        raiz->chaves[0] = chave;
        raiz->n = 1;
        return raiz;
    }

    Resultado res = inserirRec(raiz, chave);

    if (!res.promoveu)
        return raiz;

    // criar nova raiz
    No* novaRaiz = criarNo(0);

    novaRaiz->chaves[0] = res.chavePromovida;
    novaRaiz->filhos[0] = raiz;
    novaRaiz->filhos[1] = res.novoNo;
    novaRaiz->n = 1;

    return novaRaiz;
}


void imprimirArvore(No* raiz) {

    if (raiz == NULL) {
        printf("Arvore vazia\n");
        return;
    }

    No* fila[100];
    int inicio = 0, fim = 0;

    fila[fim++] = raiz;

    while (inicio < fim) {

        int tamanhoNivel = fim - inicio;

        for (int i = 0; i < tamanhoNivel; i++) {

            No* atual = fila[inicio++];

            printf("[ ");
            for (int j = 0; j < atual->n; j++)
                printf("%d ", atual->chaves[j]);
            printf("] ");

            if (!atual->folha) {
                for (int j = 0; j <= atual->n; j++) {
                    if (atual->filhos[j] != NULL)
                        fila[fim++] = atual->filhos[j];
                }
            }
        }

        printf("\n");
    }
}

int removerDaFolha(No* folha, int chave) {

    int i;

    for (i = 0; i < folha->n; i++) {
        if (folha->chaves[i] == chave)
            break;
    }

    if (i == folha->n)
        return 0; // não encontrou

    for (int j = i; j < folha->n - 1; j++) {
        folha->chaves[j] = folha->chaves[j + 1];
    }

    folha->n--;

    return 1;
}

ResultadoRemocao removerRec(No* no, int chave) {

    ResultadoRemocao res;
    res.underflow = 0;

    // 🔹 CASO FOLHA
    if (no->folha) {

        int removido = removerDaFolha(no, chave);

        if (!removido) {
            printf("Chave nao encontrada\n");
            return res;
        }

        int min = (PFOLHA + 1) / 2;

        if (no->n < min)
            res.underflow = 1;

        return res;
    }

    // 🔹 CASO INTERNO
    int pos = encontrarPosicao(no, chave);

    ResultadoRemocao resFilho = removerRec(no->filhos[pos], chave);

        // 👇 Atualização de separador
    No* filho = no->filhos[pos];

    if (filho->folha && filho->n > 0 && pos > 0) {
        no->chaves[pos - 1] = filho->chaves[0];
    }

    if (!resFilho.underflow)
        return res;

    // corrigir underflow do filho
    corrigirUnderflow(no, pos);

    int minInterno = (P + 1) / 2 - 1;

    if (no->n < minInterno)
        res.underflow = 1;

    return res;
}

int corrigirUnderflow(No* pai, int indiceFilho) {

    No* filho = pai->filhos[indiceFilho];

    No* irmaoEsq = NULL;
    No* irmaoDir = NULL;

    if (indiceFilho > 0)
        irmaoEsq = pai->filhos[indiceFilho - 1];

    if (indiceFilho < pai->n)
        irmaoDir = pai->filhos[indiceFilho + 1];

    /* =======================================================
       🔹 CASO 1 — FILHO É FOLHA
    ======================================================= */

    if (filho->folha) {

        int min = (PFOLHA + 1) / 2;

        // 1️⃣ REDISTRIBUIÇÃO COM ESQUERDO
        if (irmaoEsq && irmaoEsq->n > min) {

            int maior = irmaoEsq->chaves[irmaoEsq->n - 1];
            irmaoEsq->n--;

            for (int j = filho->n; j > 0; j--)
                filho->chaves[j] = filho->chaves[j - 1];

            filho->chaves[0] = maior;
            filho->n++;

            pai->chaves[indiceFilho - 1] = filho->chaves[0];

            return 1;
        }

        // 2️⃣ REDISTRIBUIÇÃO COM DIREITO
        if (irmaoDir && irmaoDir->n > min) {

            int menor = irmaoDir->chaves[0];

            for (int j = 0; j < irmaoDir->n - 1; j++)
                irmaoDir->chaves[j] = irmaoDir->chaves[j + 1];

            irmaoDir->n--;

            filho->chaves[filho->n] = menor;
            filho->n++;

            pai->chaves[indiceFilho] = irmaoDir->chaves[0];

            return 1;
        }

        // 3️⃣ MERGE
        if (irmaoEsq) {

            // esquerdo absorve filho
            for (int j = 0; j < filho->n; j++)
                irmaoEsq->chaves[irmaoEsq->n + j] = filho->chaves[j];

            irmaoEsq->n += filho->n;
            irmaoEsq->prox = filho->prox;

            // remover chave do pai
            for (int j = indiceFilho - 1; j < pai->n - 1; j++)
                pai->chaves[j] = pai->chaves[j + 1];

            // remover ponteiro
            for (int j = indiceFilho; j < pai->n; j++)
                pai->filhos[j] = pai->filhos[j + 1];

            pai->n--;
            free(filho);

            return 1;
        }
        else if (irmaoDir) {

            // filho absorve direito
            for (int j = 0; j < irmaoDir->n; j++)
                filho->chaves[filho->n + j] = irmaoDir->chaves[j];

            filho->n += irmaoDir->n;
            filho->prox = irmaoDir->prox;

            for (int j = indiceFilho; j < pai->n - 1; j++)
                pai->chaves[j] = pai->chaves[j + 1];

            for (int j = indiceFilho + 1; j < pai->n; j++)
                pai->filhos[j] = pai->filhos[j + 1];

            pai->n--;
            free(irmaoDir);

            return 1;
        }
    }

    /* =======================================================
       🔹 CASO 2 — FILHO É INTERNO
    ======================================================= */

    else {

        int minInterno = (P + 1) / 2 - 1;

        // 1️⃣ REDISTRIBUIÇÃO ESQUERDO
        if (irmaoEsq && irmaoEsq->n > minInterno) {

            for (int j = filho->n; j > 0; j--)
                filho->chaves[j] = filho->chaves[j - 1];

            for (int j = filho->n + 1; j > 0; j--)
                filho->filhos[j] = filho->filhos[j - 1];

            filho->chaves[0] = pai->chaves[indiceFilho - 1];
            filho->filhos[0] = irmaoEsq->filhos[irmaoEsq->n];

            filho->n++;

            pai->chaves[indiceFilho - 1] =
                irmaoEsq->chaves[irmaoEsq->n - 1];

            irmaoEsq->n--;

            return 1;
        }

        // 2️⃣ REDISTRIBUIÇÃO DIREITO
        if (irmaoDir && irmaoDir->n > minInterno) {

            filho->chaves[filho->n] =
                pai->chaves[indiceFilho];

            filho->filhos[filho->n + 1] =
                irmaoDir->filhos[0];

            filho->n++;

            pai->chaves[indiceFilho] =
                irmaoDir->chaves[0];

            for (int j = 0; j < irmaoDir->n - 1; j++)
                irmaoDir->chaves[j] = irmaoDir->chaves[j + 1];

            for (int j = 0; j < irmaoDir->n; j++)
                irmaoDir->filhos[j] = irmaoDir->filhos[j + 1];

            irmaoDir->n--;

            return 1;
        }

        // 3️⃣ MERGE
        if (irmaoEsq) {

            int base = irmaoEsq->n;

            irmaoEsq->chaves[base] =
                pai->chaves[indiceFilho - 1];

            for (int j = 0; j < filho->n; j++)
                irmaoEsq->chaves[base + 1 + j] =
                    filho->chaves[j];

            for (int j = 0; j <= filho->n; j++)
                irmaoEsq->filhos[base + 1 + j] =
                    filho->filhos[j];

            irmaoEsq->n += 1 + filho->n;

            for (int j = indiceFilho - 1; j < pai->n - 1; j++)
                pai->chaves[j] = pai->chaves[j + 1];

            for (int j = indiceFilho; j < pai->n; j++)
                pai->filhos[j] = pai->filhos[j + 1];

            pai->n--;
            free(filho);

            return 1;
        }
        else if (irmaoDir) {

            int base = filho->n;

            filho->chaves[base] =
                pai->chaves[indiceFilho];

            for (int j = 0; j < irmaoDir->n; j++)
                filho->chaves[base + 1 + j] =
                    irmaoDir->chaves[j];

            for (int j = 0; j <= irmaoDir->n; j++)
                filho->filhos[base + 1 + j] =
                    irmaoDir->filhos[j];

            filho->n += 1 + irmaoDir->n;

            for (int j = indiceFilho; j < pai->n - 1; j++)
                pai->chaves[j] = pai->chaves[j + 1];

            for (int j = indiceFilho + 1; j < pai->n; j++)
                pai->filhos[j] = pai->filhos[j + 1];

            pai->n--;
            free(irmaoDir);

            return 1;
        }
    }

    return 0;
}

void remover(No** raiz, int chave) {
    if (raiz == NULL || *raiz == NULL)
        return;

    // chama a recursiva que faz a remoção e corrige underflow
    ResultadoRemocao res = removerRec(*raiz, chave);

    // Ajusta altura: se raiz interna ficou sem chaves, promove o primeiro filho
    if (!(*raiz)->folha && (*raiz)->n == 0) {
        No* antiga = *raiz;
        *raiz = (*raiz)->filhos[0];
        free(antiga);
        return;
    }

    // Se raiz é folha e ficou vazia, libera e define NULL
    if ((*raiz)->folha && (*raiz)->n == 0) {
        free(*raiz);
        *raiz = NULL;
    }
}