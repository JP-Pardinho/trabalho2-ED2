/*
    ESTRUTURA DA DADOS II - AVALIAÇÃO 2
    PROFESSORA: Dra. Luciana Lee
    ALUNOS: 
        - 2023201331 | Gabriel dos Santos Lima
        - 2023201073 | João Pedro Pardinho Rodrigues
        - 2023200798 | Nicolas Leal Espindula
*/

#include "Bplus.h"

typedef struct {
    long enderecoProprio;
    bool ehFolha;
    int qtdElementos;
    unsigned char chaves[MAX_CHAVES + 1][TAM_MAX_CHAVE];
    long filhos[ORDEM + 1];
    long registrosDados[MAX_CHAVES + 1];
    long proximaFolha;
} Pagina;

typedef struct {
    long raiz;
    long proximoBlocoLivre;
} CabecalhoDisco;

typedef struct {
    bool houveSplit;
    unsigned char chavePromovida[TAM_MAX_CHAVE];
    long novoFilhoDireito;
} ResultadoSplit;

void tratarOverflow(ArvoreBPlus *arv, Pagina *folha, long folhaEnd, int altura, long pilhaEnderecos[]);
bool tratarUnderflow(ArvoreBPlus *arv, Pagina *atual, int nivel, long pilhaEnderecos[], int pilhaIndices[]);

/**
 *  Calcula o tamanho máximo em bytes necessário para uma página (nó) da árvore B+.
 * 
 *  retorno: (long) O tamanho calculado em bytes.
 */
long calcularTamanhoPagina() {
    long tamanhoBase = sizeof(bool) + sizeof(int);
    tamanhoBase += (MAX_CHAVES * TAM_MAX_CHAVE);

    long tamanhoInterno = tamanhoBase + (sizeof(long) * ORDEM);
    long tamanhoFolha = tamanhoBase + (sizeof(long) * MAX_CHAVES) + sizeof(long);

    return (tamanhoInterno > tamanhoFolha) ? tamanhoInterno : tamanhoFolha;
}

/**
 *  Salva uma página em disco no seu respectivo endereço.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: pag (const Pagina*) Ponteiro para a página que será salva.
 *  retorno: (void) Não retorna valor.
 */
void salvarPaginaDisco(ArvoreBPlus *arv, const Pagina *pag) {
    unsigned char *buffer = (unsigned char *)calloc(1, arv->tamanhoNoBytes);
    unsigned char *cursor = buffer;

    memcpy(cursor, &pag->ehFolha, sizeof(bool));
    cursor += sizeof(bool);
    memcpy(cursor, &pag->qtdElementos, sizeof(int));
    cursor += sizeof(int);
    memcpy(cursor, pag->chaves, MAX_CHAVES * TAM_MAX_CHAVE);
    cursor += (MAX_CHAVES * TAM_MAX_CHAVE);

    if (pag->ehFolha)
    {
        memcpy(cursor, pag->registrosDados, sizeof(long) * MAX_CHAVES);
        cursor += sizeof(long) * MAX_CHAVES;
        memcpy(cursor, &pag->proximaFolha, sizeof(long));
    }
    else
    {
        memcpy(cursor, pag->filhos, sizeof(long) * ORDEM);
    }

    fseek(arv->arquivoDisco, pag->enderecoProprio, SEEK_SET);
    fwrite(buffer, arv->tamanhoNoBytes, 1, arv->arquivoDisco);
    fflush(arv->arquivoDisco);
    free(buffer);
}

/**
 *  Carrega uma página do disco para a memória a partir de um endereço específico.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: endereco (long) Endereço (offset) da página no arquivo.
 *  parametro: pag (Pagina*) Ponteiro para a estrutura Pagina onde os dados serão armazenados.
 *  retorno: (void) Não retorna valor.
 */
void carregarPaginaDisco(ArvoreBPlus *arv, long endereco, Pagina *pag) {
    unsigned char *buffer = (unsigned char *)malloc(arv->tamanhoNoBytes);

    fseek(arv->arquivoDisco, endereco, SEEK_SET);
    fread(buffer, arv->tamanhoNoBytes, 1, arv->arquivoDisco);

    unsigned char *cursor = buffer;
    memcpy(&pag->ehFolha, cursor, sizeof(bool));
    cursor += sizeof(bool);
    memcpy(&pag->qtdElementos, cursor, sizeof(int));
    cursor += sizeof(int);
    memcpy(pag->chaves, cursor, MAX_CHAVES * TAM_MAX_CHAVE);
    cursor += (MAX_CHAVES * TAM_MAX_CHAVE);

    if (pag->ehFolha)
    {
        memcpy(pag->registrosDados, cursor, sizeof(long) * MAX_CHAVES);
        cursor += sizeof(long) * MAX_CHAVES;
        memcpy(&pag->proximaFolha, cursor, sizeof(long));
        memset(pag->filhos, 0, sizeof(pag->filhos));
    }
    else
    {
        memcpy(pag->filhos, cursor, sizeof(long) * ORDEM);
        memset(pag->registrosDados, 0, sizeof(pag->registrosDados));
        pag->proximaFolha = -1;
    }

    pag->enderecoProprio = endereco;
    free(buffer);
}

/**
 *  Atualiza o cabeçalho da árvore no arquivo de disco.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  retorno: (void) Não retorna valor.
 */
void atualizarCabecalho(ArvoreBPlus *arv) {
    CabecalhoDisco cab = {arv->enderecoRaiz, arv->proximoBlocoLivre};
    fseek(arv->arquivoDisco, 0, SEEK_SET);
    fwrite(&cab, sizeof(CabecalhoDisco), 1, arv->arquivoDisco);
    fflush(arv->arquivoDisco);
}

/**
 *  Inicializa a árvore B+ carregando uma existente do disco ou criando uma nova.
 * 
 *  parametro: caminho (const char*) Caminho para o arquivo da árvore no disco.
 *  parametro: cmp (CompararChaves) Função para comparar duas chaves.
 *  parametro: gravar (GravarChave) Função para serializar uma chave em um buffer.
 *  parametro: ler (LerChave) Função para desserializar uma chave de um buffer.
 *  parametro: tam (TamanhoChave) Função que retorna o tamanho da chave em bytes.
 *  parametro: lib (LiberarChave) Função para liberar a memória de uma chave.
 *  parametro: imp (ImprimirChave) Função para imprimir uma chave.
 *  retorno: (ArvoreBPlus*) Ponteiro para a árvore criada ou carregada.
 */
ArvoreBPlus *criarArvore(const char *caminho, CompararChaves cmp, GravarChave gravar, LerChave ler, TamanhoChave tam, LiberarChave lib, ImprimirChave imp) {

    ArvoreBPlus *arv = (ArvoreBPlus *)malloc(sizeof(ArvoreBPlus));
    arv->comparar = cmp;
    arv->gravar = gravar;
    arv->ler = ler;
    arv->tamanho = tam;
    arv->liberar = lib;
    arv->imprimir = imp;
    arv->tamanhoNoBytes = calcularTamanhoPagina();

    FILE *teste = fopen(caminho, "rb");
    if (teste)
    {
        fclose(teste);
        arv->arquivoDisco = fopen(caminho, "rb+");
        CabecalhoDisco cab;
        fseek(arv->arquivoDisco, 0, SEEK_SET);
        fread(&cab, sizeof(CabecalhoDisco), 1, arv->arquivoDisco);
        arv->enderecoRaiz = cab.raiz;
        arv->proximoBlocoLivre = cab.proximoBlocoLivre;
    }
    else
    {
        arv->arquivoDisco = fopen(caminho, "wb+");
        arv->enderecoRaiz = -1;
        arv->proximoBlocoLivre = sizeof(CabecalhoDisco);
        atualizarCabecalho(arv);
    }
    return arv;
}

/**
 *  Atualiza o cabeçalho e fecha o arquivo em disco, liberando a estrutura da árvore.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a árvore a ser fechada.
 *  retorno: (void) Não retorna valor.
 */
void fecharArvore(ArvoreBPlus *arv) {
    if (!arv)
        return;
    atualizarCabecalho(arv);
    fclose(arv->arquivoDisco);
    free(arv);
}

/**
 *  Busca uma chave na árvore B+ e opcionalmente retorna seu endereço no arquivo de dados.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: chave (const void*) Ponteiro para a chave a ser buscada.
 *  parametro: enderecoRetorno (long*) Ponteiro onde o endereço do registro será retornado (opcional, pode ser NULL).
 *  retorno: (bool) True se a chave for encontrada, False caso contrário.
 */
bool buscarChave(ArvoreBPlus *arv, const void *chave, long *enderecoRetorno) {
    if (arv->enderecoRaiz == -1)
        return false;

    long endAtual = arv->enderecoRaiz;
    Pagina pag;

    while (true)
    {
        carregarPaginaDisco(arv, endAtual, &pag);

        if (pag.ehFolha)
        {
            for (int i = 0; i < pag.qtdElementos; i++)
            {
                void *chaveDeserializada = arv->ler(pag.chaves[i]);
                int resultado = arv->comparar(chave, chaveDeserializada);
                arv->liberar(chaveDeserializada);

                if (resultado == 0)
                {
                    if (enderecoRetorno)
                        *enderecoRetorno = pag.registrosDados[i];
                    return true;
                }
            }
            return false;
        }

        int i = 0;
        while (i < pag.qtdElementos)
        {
            void *chaveDeserializada = arv->ler(pag.chaves[i]);
            int resultado = arv->comparar(chave, chaveDeserializada);
            arv->liberar(chaveDeserializada);
            if (resultado < 0)
                break;
            i++;
        }
        endAtual = pag.filhos[i];
    }
}

/**
 *  Aloca o endereço para uma nova página (nó) na árvore.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  retorno: (long) O endereço (offset) alocado para a nova página.
 */
long alocarPagina(ArvoreBPlus *arv) {
    long endereco = arv->proximoBlocoLivre;
    arv->proximoBlocoLivre += arv->tamanhoNoBytes;
    return endereco;
}

/**
 *  Desce pela árvore a partir da raiz até encontrar a folha apropriada para uma chave.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: chave (const void*) Ponteiro para a chave buscada.
 *  parametro: pilhaEnderecos (long*) Pilha que armazena os endereços dos nós visitados (opcional).
 *  parametro: pilhaIndices (int*) Pilha que armazena os índices seguidos nos nós (opcional).
 *  parametro: altura (int*) Ponteiro onde será armazenada a altura da árvore percorrida.
 *  retorno: (long) O endereço da página folha encontrada.
 */
long descerParaFolha(ArvoreBPlus *arv, const void *chave, long *pilhaEnderecos, int *pilhaIndices, int *altura) {
    long endAtual = arv->enderecoRaiz;
    *altura = 0;

    while (true)
    {
        Pagina pag;
        carregarPaginaDisco(arv, endAtual, &pag);
        if (pag.ehFolha)
            return endAtual;

        int i = 0;
        while (i < pag.qtdElementos)
        {
            void *k = arv->ler(pag.chaves[i]);
            int cmp = arv->comparar(chave, k);
            arv->liberar(k);
            if (cmp < 0)
                break;
            i++;
        }

        if (pilhaEnderecos)
            pilhaEnderecos[*altura] = endAtual;
        if (pilhaIndices)
            pilhaIndices[*altura] = i;
        (*altura)++;
        endAtual = pag.filhos[i];
    }
}

/**
 *  Insere de forma ordenada uma chave e seu registro em uma página folha.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: folha (Pagina*) Ponteiro para a página folha onde a inserção ocorrerá.
 *  parametro: chave (const void*) Ponteiro para a chave a ser inserida.
 *  parametro: enderecoRegistro (long) Endereço do registro de dados associado.
 *  retorno: (void) Não retorna valor.
 */
void inserirEmFolhaOrdenado(ArvoreBPlus *arv, Pagina *folha, const void *chave, long enderecoRegistro) {
    int i = folha->qtdElementos - 1;
    unsigned char bufChave[TAM_MAX_CHAVE];
    memset(bufChave, 0, TAM_MAX_CHAVE);
    arv->gravar(chave, bufChave);

    while (i >= 0)
    {
        void *k = arv->ler(folha->chaves[i]);
        int cmp = arv->comparar(chave, k);
        arv->liberar(k);
        if (cmp > 0)
            break;

        memcpy(folha->chaves[i + 1], folha->chaves[i], TAM_MAX_CHAVE);
        folha->registrosDados[i + 1] = folha->registrosDados[i];
        i--;
    }
    memcpy(folha->chaves[i + 1], bufChave, TAM_MAX_CHAVE);
    folha->registrosDados[i + 1] = enderecoRegistro;
    folha->qtdElementos++;
}

/**
 *  Insere de forma ordenada uma chave e o ponteiro para o filho direito em um nó interno.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: interno (Pagina*) Ponteiro para o nó interno onde a inserção ocorrerá.
 *  parametro: chave (const void*) Ponteiro para a chave a ser inserida.
 *  parametro: filhoDireito (long) Endereço do filho direito associado a esta chave.
 *  retorno: (void) Não retorna valor.
 */
void inserirEmInternoOrdenado(ArvoreBPlus *arv, Pagina *interno, const void *chave, long filhoDireito) {
    int i = interno->qtdElementos - 1;
    unsigned char bufChave[TAM_MAX_CHAVE];
    memset(bufChave, 0, TAM_MAX_CHAVE);
    arv->gravar(chave, bufChave);

    while (i >= 0)
    {
        void *k = arv->ler(interno->chaves[i]);
        int cmp = arv->comparar(chave, k);
        arv->liberar(k);
        if (cmp > 0)
            break;

        memcpy(interno->chaves[i + 1], interno->chaves[i], TAM_MAX_CHAVE);
        interno->filhos[i + 2] = interno->filhos[i + 1];
        i--;
    }
    memcpy(interno->chaves[i + 1], bufChave, TAM_MAX_CHAVE);
    interno->filhos[i + 2] = filhoDireito;
    interno->qtdElementos++;
}

/**
 *  Realiza a divisão (split) de uma página folha que está cheia (overflow).
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: folha (Pagina*) Ponteiro para a página folha a ser dividida.
 *  retorno: (ResultadoSplit) Estrutura contendo o resultado da divisão, incluindo a chave promovida.
 */
ResultadoSplit splitFolha(ArvoreBPlus *arv, Pagina *folha) {
    ResultadoSplit res;
    res.houveSplit = true;

    int total = folha->qtdElementos;
    int meio = (total + 1) / 2;

    Pagina direita;
    direita.enderecoProprio = alocarPagina(arv);
    direita.ehFolha = true;
    direita.qtdElementos = total - meio;
    memset(direita.chaves, 0, sizeof(direita.chaves));
    memset(direita.registrosDados, 0, sizeof(direita.registrosDados));

    for (int i = meio; i < total; i++)
    {
        memcpy(direita.chaves[i - meio], folha->chaves[i], TAM_MAX_CHAVE);
        direita.registrosDados[i - meio] = folha->registrosDados[i];
    }
    direita.proximaFolha = folha->proximaFolha;

    folha->qtdElementos = meio;
    folha->proximaFolha = direita.enderecoProprio;

    salvarPaginaDisco(arv, folha);
    salvarPaginaDisco(arv, &direita);

    memcpy(res.chavePromovida, direita.chaves[0], TAM_MAX_CHAVE);
    res.novoFilhoDireito = direita.enderecoProprio;
    return res;
}

/**
 *  Realiza a divisão (split) de um nó interno que está cheio (overflow).
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: interno (Pagina*) Ponteiro para o nó interno a ser dividido.
 *  retorno: (ResultadoSplit) Estrutura contendo o resultado da divisão e a chave promovida.
 */
ResultadoSplit splitInterno(ArvoreBPlus *arv, Pagina *interno) {
    ResultadoSplit res;
    res.houveSplit = true;

    int total = interno->qtdElementos;
    int meio = total / 2;

    memcpy(res.chavePromovida, interno->chaves[meio], TAM_MAX_CHAVE);

    Pagina direita;
    direita.enderecoProprio = alocarPagina(arv);
    direita.ehFolha = false;
    direita.qtdElementos = total - meio - 1;
    memset(direita.chaves, 0, sizeof(direita.chaves));
    memset(direita.filhos, 0, sizeof(direita.filhos));
    direita.proximaFolha = -1;

    for (int i = meio + 1; i < total; i++)
    {
        memcpy(direita.chaves[i - meio - 1], interno->chaves[i], TAM_MAX_CHAVE);
    }
    for (int i = meio + 1; i <= total; i++)
    {
        direita.filhos[i - meio - 1] = interno->filhos[i];
    }

    interno->qtdElementos = meio;

    salvarPaginaDisco(arv, interno);
    salvarPaginaDisco(arv, &direita);

    res.novoFilhoDireito = direita.enderecoProprio;
    return res;
}

/**
 *  Cria uma nova raiz para a árvore quando o nó raiz atual sofre split.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: filhoEsquerdo (long) Endereço da raiz antiga que virou o filho esquerdo.
 *  parametro: chaveBuf (const unsigned char*) Buffer contendo a chave promovida.
 *  parametro: filhoDireito (long) Endereço da nova página criada no split (filho direito).
 *  retorno: (void) Não retorna valor.
 */
void criarNovaRaiz(ArvoreBPlus *arv, long filhoEsquerdo, const unsigned char *chaveBuf, long filhoDireito) {
    Pagina novaRaiz;
    novaRaiz.enderecoProprio = alocarPagina(arv);
    novaRaiz.ehFolha = false;
    novaRaiz.qtdElementos = 1;
    memset(novaRaiz.chaves, 0, sizeof(novaRaiz.chaves));
    memcpy(novaRaiz.chaves[0], chaveBuf, TAM_MAX_CHAVE);
    memset(novaRaiz.filhos, 0, sizeof(novaRaiz.filhos));
    novaRaiz.filhos[0] = filhoEsquerdo;
    novaRaiz.filhos[1] = filhoDireito;
    novaRaiz.proximaFolha = -1;

    salvarPaginaDisco(arv, &novaRaiz);
    arv->enderecoRaiz = novaRaiz.enderecoProprio;
}

/**
 *  Insere uma nova chave e seu endereço de registro na árvore B+.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: chave (const void*) Ponteiro para a chave a ser inserida.
 *  parametro: enderecoRegistroRh (long) Endereço do registro de dados associado a essa chave.
 *  retorno: (bool) True se inserido com sucesso, False se a chave já existir.
 */
bool inserirChave(ArvoreBPlus *arv, const void *chave, long enderecoRegistroRh) {
    if (buscarChave(arv, chave, NULL))
        return false;

    // Caso 1: Árvore vazia, cria a primeira página (raiz)
    if (arv->enderecoRaiz == -1)
    {
        Pagina raiz;
        raiz.enderecoProprio = alocarPagina(arv);
        raiz.ehFolha = true;
        raiz.qtdElementos = 0;
        memset(raiz.chaves, 0, sizeof(raiz.chaves));
        memset(raiz.registrosDados, 0, sizeof(raiz.registrosDados));
        raiz.proximaFolha = -1;

        inserirEmFolhaOrdenado(arv, &raiz, chave, enderecoRegistroRh);
        salvarPaginaDisco(arv, &raiz);
        arv->enderecoRaiz = raiz.enderecoProprio;
        atualizarCabecalho(arv);
        return true;
    }

    // Caso 2: Árvore já existe, desce até a folha correta
    long pilhaEnderecos[64];
    int pilhaIndices[64];
    int altura = 0;

    long folhaEnd = descerParaFolha(arv, chave, pilhaEnderecos, pilhaIndices, &altura);

    Pagina folha;
    carregarPaginaDisco(arv, folhaEnd, &folha);
    inserirEmFolhaOrdenado(arv, &folha, chave, enderecoRegistroRh);

    // Se ainda couber na folha, salva e encerra
    if (folha.qtdElementos <= MAX_CHAVES)
    {
        salvarPaginaDisco(arv, &folha);
        return true;
    }

    // Caso 3: A folha excedeu a capacidade. Chama o tratamento de Overflow!
    tratarOverflow(arv, &folha, folhaEnd, altura, pilhaEnderecos);

    return true;
}

/**
 *  Remove uma chave de uma página folha.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: folha (Pagina*) Ponteiro para a folha de onde a chave será removida.
 *  parametro: chave (const void*) Ponteiro para a chave a ser removida.
 *  retorno: (void) Não retorna valor.
 */
void removerDeFolha(ArvoreBPlus *arv, Pagina *folha, const void *chave) {
    int pos = -1;
    for (int i = 0; i < folha->qtdElementos; i++)
    {
        void *k = arv->ler(folha->chaves[i]);
        int cmp = arv->comparar(chave, k);
        arv->liberar(k);
        if (cmp == 0)
        {
            pos = i;
            break;
        }
    }
    if (pos == -1)
        return;
    for (int i = pos; i < folha->qtdElementos - 1; i++)
    {
        memcpy(folha->chaves[i], folha->chaves[i + 1], TAM_MAX_CHAVE);
        folha->registrosDados[i] = folha->registrosDados[i + 1];
    }
    folha->qtdElementos--;
}

/**
 *  Remove uma chave e o respectivo filho de um nó interno, dado um índice.
 * 
 *  parametro: interno (Pagina*) Ponteiro para o nó interno.
 *  parametro: pos (int) Índice da chave a ser removida.
 *  retorno: (void) Não retorna valor.
 */
void removerDeInterno(Pagina *interno, int pos) {
    for (int i = pos; i < interno->qtdElementos - 1; i++)
    {
        memcpy(interno->chaves[i], interno->chaves[i + 1], TAM_MAX_CHAVE);
    }
    for (int i = pos + 1; i < interno->qtdElementos; i++)
    {
        interno->filhos[i] = interno->filhos[i + 1];
    }
    interno->qtdElementos--;
}

/**
 *  Tenta resolver underflow emprestando um elemento da página irmã à esquerda.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: atual (Pagina*) A página que está em underflow.
 *  parametro: esq (Pagina*) A página irmã imediatamente à esquerda.
 *  parametro: pai (Pagina*) O nó pai de ambas as páginas.
 *  parametro: idxNoPai (int) O índice que aponta para a página atual no nó pai.
 *  retorno: (bool) True se o empréstimo foi bem-sucedido, False caso a página esquerda não tenha elementos suficientes.
 */
bool emprestarDaEsquerda(ArvoreBPlus *arv, Pagina *atual, Pagina *esq, Pagina *pai, int idxNoPai) {
    if (esq->qtdElementos <= MIN_CHAVES)
        return false;

    if (atual->ehFolha)
    {
        for (int i = atual->qtdElementos; i > 0; i--)
        {
            memcpy(atual->chaves[i], atual->chaves[i - 1], TAM_MAX_CHAVE);
            atual->registrosDados[i] = atual->registrosDados[i - 1];
        }
        memcpy(atual->chaves[0], esq->chaves[esq->qtdElementos - 1], TAM_MAX_CHAVE);
        atual->registrosDados[0] = esq->registrosDados[esq->qtdElementos - 1];
        atual->qtdElementos++;
        esq->qtdElementos--;
        memcpy(pai->chaves[idxNoPai - 1], atual->chaves[0], TAM_MAX_CHAVE);
    }
    else
    {
        for (int i = atual->qtdElementos; i > 0; i--)
        {
            memcpy(atual->chaves[i], atual->chaves[i - 1], TAM_MAX_CHAVE);
        }
        for (int i = atual->qtdElementos + 1; i > 0; i--)
        {
            atual->filhos[i] = atual->filhos[i - 1];
        }
        memcpy(atual->chaves[0], pai->chaves[idxNoPai - 1], TAM_MAX_CHAVE);
        atual->filhos[0] = esq->filhos[esq->qtdElementos];
        atual->qtdElementos++;
        memcpy(pai->chaves[idxNoPai - 1], esq->chaves[esq->qtdElementos - 1], TAM_MAX_CHAVE);
        esq->qtdElementos--;
    }
    salvarPaginaDisco(arv, esq);
    salvarPaginaDisco(arv, atual);
    salvarPaginaDisco(arv, pai);
    return true;
}

/**
 *  Tenta resolver underflow emprestando um elemento da página irmã à direita.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: atual (Pagina*) A página que está em underflow.
 *  parametro: dir (Pagina*) A página irmã imediatamente à direita.
 *  parametro: pai (Pagina*) O nó pai de ambas as páginas.
 *  parametro: idxNoPai (int) O índice que aponta para a página atual no nó pai.
 *  retorno: (bool) True se o empréstimo foi bem-sucedido, False caso a página direita não tenha elementos suficientes.
 */
bool emprestarDaDireita(ArvoreBPlus *arv, Pagina *atual, Pagina *dir, Pagina *pai, int idxNoPai) {
    if (dir->qtdElementos <= MIN_CHAVES)
        return false;

    if (atual->ehFolha)
    {
        memcpy(atual->chaves[atual->qtdElementos], dir->chaves[0], TAM_MAX_CHAVE);
        atual->registrosDados[atual->qtdElementos] = dir->registrosDados[0];
        atual->qtdElementos++;
        for (int i = 0; i < dir->qtdElementos - 1; i++)
        {
            memcpy(dir->chaves[i], dir->chaves[i + 1], TAM_MAX_CHAVE);
            dir->registrosDados[i] = dir->registrosDados[i + 1];
        }
        dir->qtdElementos--;
        memcpy(pai->chaves[idxNoPai], dir->chaves[0], TAM_MAX_CHAVE);
    }
    else
    {
        memcpy(atual->chaves[atual->qtdElementos], pai->chaves[idxNoPai], TAM_MAX_CHAVE);
        atual->filhos[atual->qtdElementos + 1] = dir->filhos[0];
        atual->qtdElementos++;
        memcpy(pai->chaves[idxNoPai], dir->chaves[0], TAM_MAX_CHAVE);
        for (int i = 0; i < dir->qtdElementos - 1; i++)
        {
            memcpy(dir->chaves[i], dir->chaves[i + 1], TAM_MAX_CHAVE);
        }
        for (int i = 0; i < dir->qtdElementos; i++)
        {
            dir->filhos[i] = dir->filhos[i + 1];
        }
        dir->qtdElementos--;
    }
    salvarPaginaDisco(arv, dir);
    salvarPaginaDisco(arv, atual);
    salvarPaginaDisco(arv, pai);
    return true;
}

/**
 *  Funde (merge) duas páginas irmãs em uma só para resolver underflow de forma definitiva.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: esq (Pagina*) A página da esquerda (onde os elementos serão acumulados).
 *  parametro: dir (Pagina*) A página da direita (cujos elementos serão movidos e ela depois ficará vazia/inutilizada).
 *  parametro: pai (Pagina*) O nó pai de ambas as páginas.
 *  parametro: idxPaiEsq (int) O índice no pai correspondente à chave que separa as duas páginas.
 *  retorno: (void) Não retorna valor.
 */
void fundirPaginas(ArvoreBPlus *arv, Pagina *esq, Pagina *dir, Pagina *pai, int idxPaiEsq) {
    if (dir->ehFolha)
    {
        for (int i = 0; i < dir->qtdElementos; i++)
        {
            memcpy(esq->chaves[esq->qtdElementos + i], dir->chaves[i], TAM_MAX_CHAVE);
            esq->registrosDados[esq->qtdElementos + i] = dir->registrosDados[i];
        }
        esq->qtdElementos += dir->qtdElementos;
        esq->proximaFolha = dir->proximaFolha;
    }
    else
    {
        memcpy(esq->chaves[esq->qtdElementos], pai->chaves[idxPaiEsq], TAM_MAX_CHAVE);
        for (int i = 0; i < dir->qtdElementos; i++)
        {
            memcpy(esq->chaves[esq->qtdElementos + 1 + i], dir->chaves[i], TAM_MAX_CHAVE);
        }
        for (int i = 0; i <= dir->qtdElementos; i++)
        {
            esq->filhos[esq->qtdElementos + 1 + i] = dir->filhos[i];
        }
        esq->qtdElementos += dir->qtdElementos + 1;
    }
    salvarPaginaDisco(arv, esq);
    removerDeInterno(pai, idxPaiEsq);
}

/**
 *  Tenta tratar o underflow subindo os níveis da árvore e realizando empréstimos ou fusões.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: atual (Pagina*) Ponteiro para a página inicial que está em underflow.
 *  parametro: nivel (int) O nível atual na pilha de percurso.
 *  parametro: pilhaEnderecos (long[]) Pilha de endereços visitados na descida.
 *  parametro: pilhaIndices (int[]) Pilha de índices seguidos na descida.
 *  retorno: (bool) Retorna true após terminar as reestruturações.
 */
bool tratarUnderflow(ArvoreBPlus *arv, Pagina *atual, int nivel, long pilhaEnderecos[], int pilhaIndices[]) {
    while (nivel >= 0)
    {
        long paiEnd = pilhaEnderecos[nivel];
        int idxNoPai = pilhaIndices[nivel];
        Pagina pai;
        carregarPaginaDisco(arv, paiEnd, &pai);

        long irmaoEsqEnd = (idxNoPai > 0) ? pai.filhos[idxNoPai - 1] : -1;
        long irmaoDirEnd = (idxNoPai < pai.qtdElementos) ? pai.filhos[idxNoPai + 1] : -1;
        bool resolvido = false;

        if (irmaoEsqEnd != -1)
        {
            Pagina esq;
            carregarPaginaDisco(arv, irmaoEsqEnd, &esq);
            resolvido = emprestarDaEsquerda(arv, atual, &esq, &pai, idxNoPai);
        }

        if (!resolvido && irmaoDirEnd != -1)
        {
            Pagina dir;
            carregarPaginaDisco(arv, irmaoDirEnd, &dir);
            resolvido = emprestarDaDireita(arv, atual, &dir, &pai, idxNoPai);
        }

        if (resolvido)
            return true;

        if (irmaoEsqEnd != -1)
        {
            Pagina esq;
            carregarPaginaDisco(arv, irmaoEsqEnd, &esq);
            fundirPaginas(arv, &esq, atual, &pai, idxNoPai - 1);
        }
        else
        {
            Pagina dir;
            carregarPaginaDisco(arv, irmaoDirEnd, &dir);
            fundirPaginas(arv, atual, &dir, &pai, idxNoPai);
        }

        if (nivel == 0)
        {
            if (pai.qtdElementos == 0)
            {
                arv->enderecoRaiz = pai.filhos[0];
            }
            else
            {
                salvarPaginaDisco(arv, &pai);
            }
            atualizarCabecalho(arv);
            return true;
        }

        if (pai.qtdElementos >= MIN_CHAVES)
        {
            salvarPaginaDisco(arv, &pai);
            return true;
        }

        salvarPaginaDisco(arv, &pai);
        *atual = pai;
        nivel--;
    }
    return true;
}

/**
 *  Trata o overflow quando um nó excede a capacidade máxima estourando splits até a raiz.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: folha (Pagina*) A folha inicial que extrapolou o limite.
 *  parametro: folhaEnd (long) O endereço da folha.
 *  parametro: altura (int) A altura percorrida.
 *  parametro: pilhaEnderecos (long[]) A pilha de endereços visitados para propagar o overflow para os pais.
 *  retorno: (void) Não retorna valor.
 */
void tratarOverflow(ArvoreBPlus *arv, Pagina *folha, long folhaEnd, int altura, long pilhaEnderecos[]) {
    // Inicia o processo quebrando a folha que estourou o limite
    ResultadoSplit res = splitFolha(arv, folha);
    int nivel = altura - 1;

    // Propaga o overflow para cima, se necessário
    while (res.houveSplit && nivel >= 0)
    {
        long paiEnd = pilhaEnderecos[nivel];
        Pagina pai;
        carregarPaginaDisco(arv, paiEnd, &pai);

        void *k = arv->ler(res.chavePromovida);
        inserirEmInternoOrdenado(arv, &pai, k, res.novoFilhoDireito);
        arv->liberar(k);

        if (pai.qtdElementos <= MAX_CHAVES)
        {
            salvarPaginaDisco(arv, &pai);
            res.houveSplit = false; // Resolvido neste nível
        }
        else
        {
            res = splitInterno(arv, &pai); // O pai também estourou, continua o loop
        }
        nivel--;
    }

    // Se o overflow chegou até a raiz, precisamos criar um novo topo para a árvore
    if (res.houveSplit)
    {
        long raizAntiga = (altura == 0) ? folhaEnd : pilhaEnderecos[0];
        criarNovaRaiz(arv, raizAntiga, res.chavePromovida, res.novoFilhoDireito);
        atualizarCabecalho(arv);
    }
}

/**
 *  Remove uma chave e seu respectivo registro da árvore B+.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: chave (const void*) Ponteiro para a chave a ser removida.
 *  retorno: (bool) True se a chave foi encontrada e removida, False se não existir na árvore.
 */
bool removerChave(ArvoreBPlus *arv, const void *chave) {
    if (arv->enderecoRaiz == -1)
        return false;
    if (!buscarChave(arv, chave, NULL))
        return false;

    long pilhaEnderecos[64];
    int pilhaIndices[64];
    int altura = 0;
    long folhaEnd = descerParaFolha(arv, chave, pilhaEnderecos, pilhaIndices, &altura);

    Pagina folha;
    carregarPaginaDisco(arv, folhaEnd, &folha);
    removerDeFolha(arv, &folha, chave);

    if (altura == 0)
    {
        if (folha.qtdElementos == 0)
        {
            arv->enderecoRaiz = -1;
        }
        else
        {
            salvarPaginaDisco(arv, &folha);
        }
        atualizarCabecalho(arv);
        return true;
    }

    if (folha.qtdElementos >= MIN_CHAVES)
    {
        salvarPaginaDisco(arv, &folha);
        return true;
    }

    return tratarUnderflow(arv, &folha, altura - 1, pilhaEnderecos, pilhaIndices);
}

/**
 *  Busca todas as chaves dentro de um intervalo fechado [chaveA, chaveB] e aplica uma função de callback.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: chaveA (const void*) Ponteiro para a chave inferior do intervalo.
 *  parametro: chaveB (const void*) Ponteiro para a chave superior do intervalo.
 *  parametro: visitar (VisitarNo) Função de callback executada para cada chave encontrada.
 *  parametro: contexto (void*) Um ponteiro para o contexto que será passado ao callback.
 *  retorno: (void) Não retorna valor.
 */
void buscarIntervalo(ArvoreBPlus *arv, const void *chaveA, const void *chaveB, VisitarNo visitar, void *contexto) {
    if (arv->enderecoRaiz == -1)
        return;

    int altura = 0;
    long folhaEnd = descerParaFolha(arv, chaveA, NULL, NULL, &altura);

    while (folhaEnd != -1)
    {
        Pagina folha;
        carregarPaginaDisco(arv, folhaEnd, &folha);

        for (int i = 0; i < folha.qtdElementos; i++)
        {
            void *k = arv->ler(folha.chaves[i]);
            int cmpA = arv->comparar(k, chaveA);
            int cmpB = arv->comparar(k, chaveB);

            if (cmpA > 0 && cmpB < 0)
            {
                visitar(k, folha.registrosDados[i], contexto);
            }
            arv->liberar(k);

            if (cmpB >= 0)
                return;
        }
        folhaEnd = folha.proximaFolha;
    }
}

/**
 *  Imprime recursivamente o conteúdo dos nós da árvore (usado para depuração).
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  parametro: endereco (long) Endereço do nó atual que está sendo visitado.
 *  parametro: nivel (int) Nível de profundidade atual na árvore, usado para formatar a indentação.
 *  retorno: (void) Não retorna valor.
 */
void imprimirNoRecursivo(ArvoreBPlus *arv, long endereco, int nivel) {
    if (endereco == -1)
        return;

    Pagina pag;
    carregarPaginaDisco(arv, endereco, &pag);

    for (int t = 0; t < nivel; t++)
        printf("    ");
    printf("%s[End=%ld] (", pag.ehFolha ? "FOLHA " : "NO-INT", endereco);

    for (int i = 0; i < pag.qtdElementos; i++)
    {
        void *k = arv->ler(pag.chaves[i]);
        if (i > 0)
            printf(" | ");
        arv->imprimir(k);
        arv->liberar(k);
    }
    printf(")\n");

    if (!pag.ehFolha)
    {
        for (int i = 0; i <= pag.qtdElementos; i++)
        {
            imprimirNoRecursivo(arv, pag.filhos[i], nivel + 1);
        }
    }
}

/**
 *  Função principal de impressão da estrutura da árvore B+. Inicia a recursão pela raiz.
 * 
 *  parametro: arv (ArvoreBPlus*) Ponteiro para a estrutura da árvore.
 *  retorno: (void) Não retorna valor.
 */
void imprimirEstruturaArvore(ArvoreBPlus *arv) {
    if (arv->enderecoRaiz == -1)
    {
        printf("(Árvore vazia)\n");
        return;
    }
    printf("=== Estrutura da Árvore B+ (raiz no endereço %ld) ===\n", arv->enderecoRaiz);
    imprimirNoRecursivo(arv, arv->enderecoRaiz, 0);
    printf("=====================================================\n");
}