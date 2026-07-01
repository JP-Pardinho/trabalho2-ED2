#include "Bplus.h"
#include <stdio.h>
#include <stdlib.h>

static const char *arquivoArvore = "arvore.dat";

// funcões para a página
Pagina *criaPagina()
{
    Pagina *pagina = (Pagina *)malloc(sizeof(Pagina));
    if (pagina == NULL)
    {
        printf("Erro ao alocar memória para a página!\n");
        return NULL;
    }
    return pagina;
}

void inicializarPagina(Pagina *pagina, int indice, int tipo)
{
    pagina->indice = indice;
    pagina->pai = -1;
    pagina->proximaFolha = -1;
    pagina->qtElementos = 0;
    pagina->ehfolha = tipo;
    pagina->foiDeletada = 0;
}

void destroiPagina(Pagina *p)
{
    p->foiDeletada = 1;
    p->qtElementos = 0;

    FILE *fp = fopen(arquivoArvore, "r+b"); // r+b permite escrita sem apagar o arquivoArvore
    if (fp != NULL)
    {
        fseek(fp, sizeof(Cabecalho) + p->indice * sizeof(Pagina), SEEK_SET);
        fwrite(p, sizeof(Pagina), 1, fp);
    }

    // agora que está salvo no disco, removemos da memória RAM
    free(p);
    // ajusta o cabecaho da arvore
    Cabecalho header;
    fseek(arquivoArvore, 0, SEEK_SET);
    fread(&header, sizeof(Cabecalho), 1, arquivoArvore);
    header.qtdPaginas--;
    fwrite(&header, sizeof(Cabecalho), 1, fp);
    fclose(fp);
}

void inserirElemento(Pagina *p, const void *chave, int indice, int (*comparar)(const void *, const void *))
{

    // Validação se é folha
    if (p->ehfolha == 0)
    {
        printf("Não é possível inserir elementos em uma página interna por essa função!\n");
        return;
    }

    // Inserção mantendo o ponteiro genérico
    p->chave[p->qtElementos] = (void *)chave;
    p->filho[p->qtElementos] = indice;
    p->qtElementos++;

    ordenarPaginaFolha(p, comparar);

    // Lógica da cisão (Corte da página se estourar a ORDEM)
    if (p->qtElementos > ORDEM)
    {

        Pagina *novaPagina = criaPagina();
        int meio = p->qtElementos / 2;

        // Move a metade superior para a nova página
        for (int j = 0, k = meio + 1; k < p->qtElementos; ++k, ++j)
        {
            novaPagina->chave[j] = p->chave[k];
            novaPagina->filho[j] = p->filho[k];
            novaPagina->qtElementos++;
        }
        int indiceNovaPagina = buscarPaginaLivre(arquivoArvore);
        novaPagina->indice = indiceNovaPagina;
        novaPagina->pai = p->pai;
        novaPagina->ehfolha = p->ehfolha;
        p->proximaFolha = novaPagina;

        // isso garante que eu não possa acesar as chaves e filhos que foram perdidos na cisão
        p->qtElementos = meio + 1;

        // escreve a paginaNova e a p no arquivoArvore
        FILE *fp = fopen(arquivoArvore, "r+b");
        if (fp != NULL)
        {
            fseek(fp, sizeof(Cabecalho) + indiceNovaPagina * sizeof(Pagina), SEEK_SET);
            fwrite(novaPagina, sizeof(Pagina), 1, fp);

            fseek(fp, sizeof(Cabecalho) + p->indice * sizeof(Pagina), SEEK_SET);
            fwrite(p, sizeof(Pagina), 1, fp);

            // atualiza o cabecalho no arquivoArvore
            Cabecalho header;
            fseek(fp, 0, SEEK_SET);
            fread(&header, sizeof(Cabecalho), 1, fp);
            header.qtdPaginas++;
            fseek(fp, 0, SEEK_SET);
            fwrite(&header, sizeof(Cabecalho), 1, fp);

            fclose(fp);
        }

        // Se NÃO for a raiz (tem pai no arquivoArvore)
        if (p->pai != -1)
        {
            FILE *fp = fopen(arquivoArvore, "r+b");
            if (fp != NULL)
            {
                fseek(fp, sizeof(Cabecalho) + p->pai * sizeof(Pagina), SEEK_SET);
                Pagina *pai = criaPagina();
                inicializarPagina(pai, p->pai, 0);
                fread(pai, sizeof(Pagina), 1, fp);
                fclose(fp);

                inserirElemento(pai, p->chave[meio], indiceNovaPagina, comparar);

                // Ajusta a quantidade de elementos que sobrou na página atual
                p->qtElementos = meio;

                // Salva o pai atualizado no arquivoArvore
                FILE *fp = fopen(arquivoArvore, "r+b");
                if (fp != NULL)
                {
                    fseek(fp, sizeof(Cabecalho) + p->pai * sizeof(Pagina), SEEK_SET);
                    fwrite(pai, sizeof(Pagina), 1, fp);
                    fclose(fp);
                }
                destroiPagina(pai);
            }
        }
        // Se FOR a raiz
        else
        {
            Pagina *novaRaiz = criaPagina();
            int indiceNovaRaiz = buscarPaginaLivre(arquivoArvore);
            inicializarPagina(novaRaiz, indiceNovaRaiz, 0);

            novaRaiz->chave[0] = p->chave[meio];
            novaRaiz->filho[0] = p->indice;
            novaRaiz->filho[1] = indiceNovaPagina; //
            novaRaiz->qtElementos = 1;

            p->pai = indiceNovaRaiz;
            novaPagina->pai = indiceNovaRaiz;

            FILE *fp = fopen(arquivoArvore, "r+b");
            if (fp != NULL)
            {
                // Atualiza p e novaPagina com o novo índice do pai
                fseek(fp, sizeof(Cabecalho) + p->indice * sizeof(Pagina), SEEK_SET);
                fwrite(p, sizeof(Pagina), 1, fp);
                fseek(fp, sizeof(Cabecalho) + indiceNovaPagina * sizeof(Pagina), SEEK_SET);
                fwrite(novaPagina, sizeof(Pagina), 1, fp);

                // adicona nova raiz ao arquivoArvore
                fseek(fp, sizeof(Cabecalho) + indiceNovaRaiz * sizeof(Pagina), SEEK_SET);
                fwrite(novaRaiz, sizeof(Pagina), 1, fp);

                // atualiza o cabecalho no arquivoArvore
                Cabecalho header;
                fseek(fp, 0, SEEK_SET);
                fread(&header, sizeof(Cabecalho), 1, fp);
                header.raiz = indiceNovaRaiz;
                header.qtdPaginas++;
                fseek(fp, 0, SEEK_SET);
                fwrite(&header, sizeof(Cabecalho), 1, fp);

                fclose(fp);
            }
            free(novaRaiz); // libera memória ram
        }
        free(novaPagina); // Libera a memória ram
    }
}

int removerElemento(Pagina *p, const void *chave, int (*comparar)(const void *, const void *))
{
    int pos = -1;

    // procura a chave na página
    for (int i = 0; i < p->qtElementos; i++)
    {
        if (comparar(chave, p->chave[i]) == 0)
        {
            pos = i;
            break;
        }
    }

    // chave não encontrada, retorna
    if (pos == -1)
    {
        return;
    }

    // Se encontrada, desloca elementos para esquerda
    for (int i = pos; i < p->qtElementos - 1; i++)
    {
        p->chave[i] = p->chave[i + 1]; // Atribuição direta de ponteiros genéricos
        p->filho[i] = p->filho[i + 1];
    }

    // limpa a última posição do vetor
    p->chave[p->qtElementos - 1] = NULL; // Definido como NULL de forma segura
    p->filho[p->qtElementos - 1] = 0;    // (ou -1, dependendo do seu padrão para "sem filho")

    p->qtElementos--;
}

void verificarOverflow(Pagina *p, int (*comparar)(const void *, const void *))
{
    if (p->qtElementos <= ORDEM)
        return;

    Cabecalho header;
    fseek(arquivoArvore, 0, SEEK_SET);
    fread(&header, sizeof(Cabecalho), 1, arquivoArvore);

    // Cria a nova página irmã que vai receber a metade dos elementos
    Pagina *novaPagina = criaPagina();
    novaPagina->indice = buscarPaginaLivre(arquivoArvore);
    novaPagina->ehfolha = p->ehfolha;
    novaPagina->pai = p->pai;

    int meio = p->qtElementos / 2;
    void *chaveMediana;
    chaveMediana = p->chave[meio];

    if (p->ehfolha)
    {
        // Se for folha, copia do meio até o fim (incluindo o elemento do meio)
        for (int i = meio, j = 0; i < p->qtElementos; i++, j++)
        {
            novaPagina->chave[j] = p->chave[i];
            novaPagina->filho[j] = p->filho[i];
            novaPagina->qtElementos++;
        }
        p->qtElementos = meio;

        // Atualiza a lista encadeada de folhas
        novaPagina->proximaFolha = p->proximaFolha;
        p->proximaFolha = novaPagina->indice;
    }
    else
    {
        // Se for nó interno, o elemento do meio SÓ SOBE, não fica na nova página
        for (int i = meio + 1, j = 0; i < p->qtElementos; i++, j++)
        {
            novaPagina->chave[j] = p->chave[i];
            novaPagina->filho[j] = p->filho[i];
            novaPagina->qtElementos++;
        }
        // O último ponteiro de filho também precisa ser copiado em nós internos
        novaPagina->filho[novaPagina->qtElementos] = p->filho[p->qtElementos];

        p->qtElementos = meio;
    }

    // Atualiza os ponteiros de pai dos filhos que mudaram de página (se for nó interno)
    if (!p->ehfolha)
    {
        for (int i = 0; i <= novaPagina->qtElementos; i++)
        {
            Pagina filhoTemp;
            fseek(arquivoArvore, sizeof(Cabecalho) + novaPagina->filho[i] * sizeof(Pagina), SEEK_SET);
            fread(&filhoTemp, sizeof(Pagina), 1, arquivoArvore);
            filhoTemp.pai = novaPagina->indice;
            fseek(arquivoArvore, sizeof(Cabecalho) + novaPagina->filho[i] * sizeof(Pagina), SEEK_SET);
            fwrite(&filhoTemp, sizeof(Pagina), 1, arquivoArvore);
        }
    }

    // Trata a subida da chave para o pai
    if (p->pai == -1)
    {
        // Caso especial: p era a raiz antiga. Cria-se uma nova raiz.
        Pagina *novaRaiz = criaPagina();
        novaRaiz->indice = buscarPaginaLivre(arquivoArvore);
        novaRaiz->ehfolha = 0; // Raiz nova com filhos deixa de ser folha
        novaRaiz->pai = -1;

        memcpy(novaRaiz->chave[0], chaveMediana, 100);
        novaRaiz->filho[0] = p->indice;
        novaRaiz->filho[1] = novaPagina->indice;
        novaRaiz->qtElementos = 1;

        // Atualiza os pais de p e novaPagina para apontarem para a nova raiz
        p->pai = novaRaiz->indice;
        novaPagina->pai = novaRaiz->indice;

        // Atualiza o cabeçalho da árvore no arquivoArvore
        header.raiz = novaRaiz->indice;
        header.qtdPaginas += 2; // Criou novaPagina e novaRaiz
        fseek(arquivoArvore, 0, SEEK_SET);
        fwrite(&header, sizeof(Cabecalho), 1, arquivoArvore);

        // Salva a nova raiz no arquivoArvore
        fseek(arquivoArvore, sizeof(Cabecalho) + novaRaiz->indice * sizeof(Pagina), SEEK_SET);
        fwrite(novaRaiz, sizeof(Pagina), 1, arquivoArvore);
        destroiPagina(novaRaiz);
    }
    else
    {
        // Se já possui pai, carrega o pai do disco e insere recursivamente
        Pagina pai;
        fseek(arquivoArvore, sizeof(Cabecalho) + p->pai * sizeof(Pagina), SEEK_SET);
        fread(&pai, sizeof(Pagina), 1, arquivoArvore);

        // Atualiza a contagem de páginas criadas no cabeçalho
        header.qtdPaginas++;
        fseek(arquivoArvore, 0, SEEK_SET);
        fwrite(&header, sizeof(Cabecalho), 1, arquivoArvore);

        // Insere a chave que subiu no pai. O "filho" associado a ela é a novaPagina.
        // Como o pai é um nó interno, burlamos temporariamente o check de folha da função de inserção
        pai.ehfolha = 1;
        inserirElemento(&pai, chaveMediana, novaPagina->indice, comparar);
        pai.ehfolha = 0; // Devolve o status original de nó interno do pai
    }

    // Salva as alterações das páginas manipuladas de volta no arquivoArvore
    fseek(arquivoArvore, sizeof(Cabecalho) + p->indice * sizeof(Pagina), SEEK_SET);
    fwrite(p, sizeof(Pagina), 1, arquivoArvore);

    fseek(arquivoArvore, sizeof(Cabecalho) + novaPagina->indice * sizeof(Pagina), SEEK_SET);
    fwrite(novaPagina, sizeof(Pagina), 1, arquivoArvore);

    destroiPagina(novaPagina);
}

void verificarUnderflow(Pagina *pagina)
{
    Cabecalho header;

    // lê cabeçalho
    fseek(arquivoArvore, 0, SEEK_SET);
    fread(&header, sizeof(Cabecalho), 1, arquivoArvore);

    int minimo = ORDEM / 2;

    // não existe underflow
    if (pagina->qtElementos >= minimo)
        return;

    // caso a página seja raiz
    if (pagina->indice == header.raiz)
    {
        // se a raiz ficou vazia, árvore fica vazia
        if (pagina->qtElementos == 0)
        {
            header.raiz = -1;

            fseek(arquivoArvore, 0, SEEK_SET);
            fwrite(&header, sizeof(Cabecalho), 1, arquivoArvore);
        }
        return;
    }

    // carrega pai
    Pagina pai;

    fseek(arquivoArvore, sizeof(Cabecalho) + pagina->pai * sizeof(Pagina), SEEK_SET);
    fread(&pai, sizeof(Pagina), 1, arquivoArvore);

    // descobre posição da página no pai
    int pos = 0;

    while (pai.filho[pos] != pagina->indice)
        pos++;

    Pagina irmaAdjacente;

    // tenta pegar da irmã esquerda
    if (pos > 0)
    {
        fseek(arquivoArvore, sizeof(Cabecalho) + pai.filho[pos - 1] * sizeof(Pagina), SEEK_SET);
        fread(&irmaAdjacente, sizeof(Pagina), 1, arquivoArvore);

        if (irmaAdjacente.qtElementos > minimo)
        {

            // move elementos da página para abrir espaço
            for (int i = pagina->qtElementos; i > 0; i--)
            {
                pagina->chave[i] = pagina->chave[i - 1];
                pagina->filho[i] = pagina->filho[i - 1];
            }
            // pega maior elemento do irmão esquerdo

            pagina->chave[0] = irmaAdjacente.chave[irmaAdjacente.qtElementos - 1];
            pagina->filho[0] = irmaAdjacente.filho[irmaAdjacente.qtElementos - 1];
            pagina->qtElementos++;
            irmaAdjacente.qtElementos--;

            // salva a irmã

            fseek(arquivoArvore, sizeof(Cabecalho) + irmaAdjacente.indice * sizeof(Pagina), SEEK_SET);
            fwrite(&irmaAdjacente, sizeof(Pagina), 1, arquivoArvore);

            return;
        }
    }

    // tenta pegar da irmã direita
    if (pos < pai.qtElementos)
    {
        fseek(arquivoArvore, sizeof(Cabecalho) + pai.filho[pos + 1] * sizeof(Pagina), SEEK_SET);
        fread(&irmaAdjacente, sizeof(Pagina), 1, arquivoArvore);

        if (irmaAdjacente.qtElementos > minimo)
        {

            pagina->chave[pagina->qtElementos] = irmaAdjacente.chave[0];
            pagina->filho[pagina->qtElementos] = irmaAdjacente.filho[0];
            pagina->qtElementos++;

            // remove primeiro elemento da irmã
            for (int i = 0; i < irmaAdjacente.qtElementos - 1; i++)
            {
                irmaAdjacente.chave[i] = irmaAdjacente.chave[i + 1];
                irmaAdjacente.filho[i] = irmaAdjacente.filho[i + 1];
            }

            irmaAdjacente.qtElementos--;

            fseek(arquivoArvore, sizeof(Cabecalho) + irmaAdjacente.indice * sizeof(Pagina), SEEK_SET);
            fwrite(&irmaAdjacente, sizeof(Pagina), 1, arquivoArvore);

            return;
        }
    }

    // se chegou aqui, não conseguiu redistribuir
    // faz concatenação !!!

    if (pos > 0)
    {

        fseek(arquivoArvore, sizeof(Cabecalho) + pai.filho[pos - 1] * sizeof(Pagina), SEEK_SET);
        fread(&irmaAdjacente, sizeof(Pagina), 1, arquivoArvore);

        for (int i = 0; i < pagina->qtElementos; i++)
        {
            irmaAdjacente.chave[irmaAdjacente.qtElementos] = pagina->chave[i];
            irmaAdjacente.filho[irmaAdjacente.qtElementos] = pagina->filho[i];
            irmaAdjacente.qtElementos++;
        }

        // marca página como removida
        pagina->foiDeletada = 1;

        fseek(arquivoArvore, sizeof(Cabecalho) + irmaAdjacente.indice * sizeof(Pagina), SEEK_SET);
        fwrite(&irmaAdjacente, sizeof(Pagina), 1, arquivoArvore);

        // remove referência no pai
        for (int i = pos; i < pai.qtElementos; i++)
        {
            pai.filho[i] = pai.filho[i + 1];
        }
        pai.qtElementos--;

        fseek(arquivoArvore, sizeof(Cabecalho) + pai.indice * sizeof(Pagina), SEEK_SET);
        fwrite(&pai, sizeof(Pagina), 1, arquivoArvore);
    }
}

void ordenarPaginaFolha(Pagina *p, int (*comparar)(const void *, const void *))
{
    // a função de ordenação vai atuar panas na memória ram
    // as funções responsáveis por chamar ela que gravam isso no disco

    int j;
    void *aux1;
    int aux2;

    for (int i = 1; i < p->qtElementos; i++)
    {
        aux1 = p->chave[i];
        aux2 = p->filho[i];

        // Loop do Insertion Sort usando a função genérica de comparação
        for (j = i; j > 0 && comparar(aux1, p->chave[j - 1]) < 0; j--)
        {
            p->chave[j] = p->chave[j - 1];
            p->filho[j] = p->filho[j - 1];
        }

        // Coloca os valores nos seus devidos lugares ordenados
        p->chave[j] = aux1;
        p->filho[j] = aux2;
    }
}

int buscarPaginaLivre()
{

    // abre o arquivoArvore pra leitura
    FILE *arquivoArvore = fopen(arquivoArvore, "rb");
    if (arquivoArvore == NULL)
    {
        printf("Erro ao abrir o arquivoArvore!\n");
        return;
    }
    // pula o cabecalho
    fseek(arquivoArvore, sizeof(Cabecalho), SEEK_SET);
    // le pagina a pagina até encontrar uma que foi deletada logicamente
    Pagina p;
    int i = 0;
    while (fread(&p, sizeof(Pagina), 1, arquivoArvore))
    {
        if (p.foiDeletada)
            break;
        i++;
    }
    // fecha o arquivoArvore
    fclose(arquivoArvore);
    // retorna indice da página já deletada
    return i;
}

// funcões para a árvore
void inicializarArvore(int ordem, int tamChave)
{

    FILE *arquivoArvore = fopen(arquivoArvore, "rb+");
    // Confere se arquivoArvore  da árvore já não foi criado
    if (arquivoArvore == NULL)
    {
        Cabecalho arvore;
        arvore.raiz = -1;
        arvore.qtdPaginas = 0;
        arvore.tamChave = tamChave;
        arvore.ordem = ordem; // podemos tirar isso e usar do define
        arvore.qtdPaginas = 0;

        FILE *arquivoArvore = fopen(arquivoArvore, "wb+");
        fwrite(&arvore, sizeof(Cabecalho), 1, arquivoArvore);
    }

    else
        printf("Arquivo da árvore já existe!\n");

    fclose(arquivoArvore);
}

void imprimirArvore();

Pagina buscarFolha(Cabecalho *header, const void *chave, int (*comparar)(const void *, const void *))
{

    FILE *arquivoArvore = fopen(arquivoArvore, "rb");
    Pagina pagina;

    // Carrega a raiz
    fseek(arquivoArvore, sizeof(Cabecalho) + header->raiz * sizeof(Pagina), SEEK_SET);
    fread(&pagina, sizeof(Pagina), 1, arquivoArvore);

    // Enquanto não chegar em uma folha
    while (pagina.ehfolha == 0)
    {
        int i = 0;

        // Descobre qual filho seguir
        while (i < pagina.qtElementos && comparar(chave, pagina.chave[i]) > 0)
            i++;

        // Carrega o filho escolhido
        fseek(arquivoArvore, sizeof(Cabecalho) + pagina.filho[i] * sizeof(Pagina), SEEK_SET);
        fread(&pagina, sizeof(Pagina), 1, arquivoArvore);
    }
    return pagina;
}

int buscarChaveNaArvore(const void *chave, int *enderecoRegistro, int (*comparar)(const void *, const void *))
{

    FILE *arquivoArvore = fopen(arquivoArvore, "rb");

    if (arquivoArvore == NULL)
    {
        printf("Erro ao abrir o arquivoArvore!\n");
        return -1;
    }

    Cabecalho header;

    if (fread(&header, sizeof(Cabecalho), 1, arquivoArvore) != 1)
    {
        printf("Erro ao ler o cabeçalho!!\n");
        fclose(arquivoArvore);
        return -1;
    }

    if (header.raiz == -1)
    {
        printf("Árvore vazia!!\n");
        fclose(arquivoArvore);
        return -1;
    }

    // busca a folha
    Pagina p = buscarFolha(&header, chave, comparar);

    // procura a chave na folha
    for (int i = 0; i < p.qtElementos; i++)
    {

        if (comparar(chave, p.chave[i]) == 0)
        {

            *enderecoRegistro = p.filho[i];

            fclose(arquivoArvore);
            return 1;
        }
    }

    fclose(arquivoArvore);

    return -1;
}

int *buscarChavesIntervalo(const void *chaveMin, const void *chaveMax, int *qtEncontrados, int (*comparar)(const void *, const void *))
{
    FILE *arquivoArvore = fopen(arquivoArvore, "rb");

    // verifica se abriu
    if (arquivoArvore == NULL)
        return NULL;

    // le cabeçalho

    Cabecalho header;

    if (fread(&header, sizeof(Cabecalho), 1, arquivoArvore) != 1)
    {
        fclose(arquivoArvore);
        return NULL;
    }

    // verifica se a árvore existe
    if (header.raiz == -1)
    {
        fclose(arquivoArvore);
        return NULL;
    }

    // Encontra a folha onde chaveMin estaria
    Pagina pagina = buscarFolha(&header, chaveMin, comparar);

    // aloca vetor com um tamanho inicial, se precisar de mais realloca
    int capacidade = 10;
    int *enderecos = (int *)malloc(capacidade * sizeof(int));
    *qtEncontrados = 0;

    bool terminou = false;

    while (!terminou)
    {

        // percorre as chaves na folha
        for (int i = 0; i < pagina.qtElementos; i++)
        {

            // se a chave encontrada for maior que a máxima, encerra pois já terminou o intervalo
            if (comparar(pagina.chave[i], chaveMax) > 0)
            {
                terminou = true;
                break;
            }

            // se está no intervalo, compara com a minima
            if (comparar(pagina.chave[i], chaveMin) >= 0)
            {
                // se a chave a página estiver entro do intervalo (chaveMin, chaveMax)

                // verifica se o vetor tem espaço suficiente
                if (*qtEncontrados == capacidade)
                {
                    capacidade *= 2;
                    enderecos = (int *)realloc(enderecos, capacidade * sizeof(int));
                }

                // salva endereço no vetor e incrementa contador
                enderecos[*qtEncontrados] = pagina.filho[i];
                (*qtEncontrados)++;
            }
        }

        if (terminou)
            break;

        // acabou a última folha?
        if (pagina.proximaFolha == -1)
            break;

        // carrega a próxima folha
        fseek(arquivoArvore, sizeof(Cabecalho) + pagina.proximaFolha * sizeof(Pagina), SEEK_SET);
        fread(&pagina, sizeof(Pagina), 1, arquivoArvore);
    }

    fclose(arquivoArvore);

    return enderecos;
}

void inserirChaveNaArvore(const void *chave, int enderecoRegistro, size_t tamanhoChave, int (*comparar)(const void *, const void *))
{

    FILE *arquivoArvore = fopen(arquivoArvore, "rb+");

    if (arquivoArvore == NULL)
    {
        printf("Erro ao abrir o arquivoArvore!\n");
        return;
    }

    Cabecalho header;
    fread(&header, sizeof(Cabecalho), 1, arquivoArvore);

    // Árvore vazia
    if (header.raiz == -1)
    {

        Pagina *raiz = criaPagina();
        inicializarPagina(raiz, 0, 1);
        inserirElemento(raiz, chave, enderecoRegistro, comparar);
        raiz->filho[0] = enderecoRegistro;
        header.raiz = 0;
        header.qtdPaginas = 1;

        fseek(arquivoArvore, 0, SEEK_SET);
        fwrite(&header, sizeof(Cabecalho), 1, arquivoArvore);

        fseek(arquivoArvore, sizeof(Cabecalho), SEEK_SET);
        fwrite(raiz, sizeof(Pagina), 1, arquivoArvore);

        destroiPagina(raiz);

        fclose(arquivoArvore);
        return;
    }

    // encontra a folha
    Pagina pagina = buscarFolha(&header, chave, comparar);

    // insere na folha
    inserirElemento(&pagina, chave, pagina.indice, comparar);
    pagina.filho[pagina.qtElementos - 1] = enderecoRegistro;
    ordenarPaginaFolha(&pagina, comparar);
    verificarOverflow(&pagina, comparar);

    // salva a página

    // ISSO A FUNÇÃO ISERIR ELEMENTO JÁ FAZ!!!!!!!!!!
    // FALAR COM AS MENINAS

    // fseek(arquivoArvore, sizeof(Cabecalho) + pagina.indice * sizeof(Pagina), SEEK_SET);
    // fwrite(&pagina, sizeof(Pagina), 1, arquivoArvore);
    // fclose(arquivoArvore);
}

void deletarChaveNaArvore(const void *chave, int (*comparar)(const void *, const void *))
{

    FILE *arquivoArvore = fopen(arquivoArvore, "rb+");

    if (arquivoArvore == NULL)
    {
        printf("Erro ao abrir o arquivoArvore!!!\n");
        return;
    }

    Cabecalho header;

    if (fread(&header, sizeof(Cabecalho), 1, arquivoArvore) != 1)
    {
        fclose(arquivoArvore);
        return;
    }

    if (header.raiz == -1)
    {
        printf("Árvore vazia!\n");
        fclose(arquivoArvore);
        return;
    }

    // encontra a folha
    Pagina pagina = buscarFolha(&header, chave, comparar);

    // tenta remover
    if (!removerElemento(&pagina, chave, comparar))
    {
        printf("Chave não encontrada.\n");
        fclose(arquivoArvore);
        return;
    }

    verificarUnderflow(&pagina);

    fseek(arquivoArvore, sizeof(Cabecalho) + pagina.indice * sizeof(Pagina), SEEK_SET);
    fwrite(&pagina, sizeof(Pagina), 1, arquivoArvore);

    fclose(arquivoArvore);
}
