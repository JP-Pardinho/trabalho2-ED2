#include "Bplus.h"
#include <stdio.h>
#include <stdlib.h>

// funcões para a página
Pagina *criaPagina(){
    Pagina *pagina = (Pagina*)malloc(sizeof(Pagina));
    if (pagina == NULL){
        printf("Erro ao alocar memória para a página!\n");
        return NULL;
    }
    return pagina;
}

void inicializarPagina(Pagina *pagina, int indice, int tipo){
    pagina->indice = indice;
    pagina->pai = -1;
    pagina->proximaFolha = -1;
    pagina->qtElementos = 0;
    pagina->ehfolha = tipo;
    pagina->foiDeletada = 0;
}

void destroiPagina(Pagina *p){ 
    // Só destroi a árvore caso seja uma folha
    if (p->ehfolha == 1){
        for (int i = 0; i < p->qtElementos; i++){
            if (p->chave[i] != NULL){
                free(p->chave[i]);
            }
        }
        p->qtElementos = 0;
        p->ehfolha = 0;
        p->foiDeletada = 1;
        // deleta logicamente a página
    }
}


void inserirElemento(Pagina *p, const void* chave, int indice){
    
    if (p->ehfolha == 0){
        printf("Não é possível inserir elementos em uma página interna!\n");
        return;
    }

    memcpy(p->chave[p->qtElementos], (const char*)chave, 100);
    p->filho[p->qtElementos] = indice;
    p->qtElementos++;
    
    ordenarPaginaFolha(p);

    if(p->qtElementos > ORDEM){

        Pagina *novaPagina = criaPagina();
        for(int j = 0, k = p->qtElementos/2 + 1; k < p->qtElementos; ++k, ++j){
            memcpy(novaPagina->chave[j], p->chave[k], 100);
        }
        
        // Se não for raiz
        if(p->pai != -1){
            FILE* fp = fopen("árvore.dat", "rb"); 
            if (fp != NULL) {
                fseek(fp, p->pai * sizeof(Pagina), SEEK_SET);
                Pagina *pai = criaPagina();
                inicializarPagina(pai, p->pai, 0);
                fread(pai, sizeof(Pagina), 1, fp); 
                fclose(fp);
                
                inserirElemento(pai, p->chave[p->qtElementos/2], -1);
                p->qtElementos = p->qtElementos/2 - 1;
                
                destroiPagina(pai); 
            }
        }
        // se for raiz
        else{
            Pagina *novaRaiz = criaPagina();
            memcpy(novaRaiz->chave[0], p->chave[p->qtElementos/2], 100);
            novaRaiz->filho[0] = p->indice;
        }
    }
}

void removerElemento(Pagina *p, const void *chave, int (*comparar)(const void*, const void*)){
    int pos = -1;

    // procura a chave na página
    for(int i = 0; i < p->qtElementos; i++){
        if(comparar(chave, p->chave[i]) == 0){
            pos = i;
            break;
        }
    }

    // chave não encontrada, retorna
    if(pos == -1){
        return;
    }

    //Se encontrada, desloca elementos para esquerda
    for(int i = pos; i < p->qtElementos - 1; i++){
        memcpy(p->chave[i],
               p->chave[i+1],
               sizeof(p->chave[i]));

        p->filho[i] = p->filho[i+1];
    }

    // limpa a última posição do vetor
    memset(&p->chave[p->qtElementos-1],
           0,
           sizeof(p->chave[p->qtElementos-1]));

    p->qtElementos--;
}

void verificarOverflow(FILE *arquivo, Pagina *p) {
    if (p->qtElementos <= ORDEM) 
        return;

    Cabecalho header;
    fseek(arquivo, 0, SEEK_SET);
    fread(&header, sizeof(Cabecalho), 1, arquivo);

    // Cria a nova página irmã que vai receber a metade dos elementos
    Pagina *novaPagina = criaPagina();
    novaPagina->indice = buscarPaginaLivre(arquivo);
    novaPagina->ehfolha = p->ehfolha;
    novaPagina->pai = p->pai;

    int meio = p->qtElementos / 2;
    unsigned char chaveMediana[100];
    memcpy(chaveMediana, p->chave[meio], 100);

    if (p->ehfolha) {
        // Se for folha, copia do meio até o fim (incluindo o elemento do meio)
        for (int i = meio, j = 0; i < p->qtElementos; i++, j++) {
            memcpy(novaPagina->chave[j], p->chave[i], 100);
            novaPagina->filho[j] = p->filho[i];
            novaPagina->qtElementos++;
        }
        p->qtElementos = meio;

        // Atualiza a lista encadeada de folhas
        novaPagina->proximaFolha = p->proximaFolha;
        p->proximaFolha = novaPagina->indice;

    } else {
        // Se for nó interno, o elemento do meio SÓ SOBE, não fica na nova página
        for (int i = meio + 1, j = 0; i < p->qtElementos; i++, j++) {
            memcpy(novaPagina->chave[j], p->chave[i], 100);
            novaPagina->filho[j] = p->filho[i];
            novaPagina->qtElementos++;
        }
        // O último ponteiro de filho também precisa ser copiado em nós internos
        novaPagina->filho[novaPagina->qtElementos] = p->filho[p->qtElementos];
        
        p->qtElementos = meio; 
    }

    // Atualiza os ponteiros de pai dos filhos que mudaram de página (se for nó interno)
    if (!p->ehfolha) {
        for (int i = 0; i <= novaPagina->qtElementos; i++) {
            Pagina filhoTemp;
            fseek(arquivo, sizeof(Cabecalho) + novaPagina->filho[i] * sizeof(Pagina), SEEK_SET);
            fread(&filhoTemp, sizeof(Pagina), 1, arquivo);
            filhoTemp.pai = novaPagina->indice;
            fseek(arquivo, sizeof(Cabecalho) + novaPagina->filho[i] * sizeof(Pagina), SEEK_SET);
            fwrite(&filhoTemp, sizeof(Pagina), 1, arquivo);
        }
    }

    // Trata a subida da chave para o pai
    if (p->pai == -1) {
        // Caso especial: p era a raiz antiga. Cria-se uma nova raiz.
        Pagina *novaRaiz = criaPagina();
        novaRaiz->indice = buscarPaginaLivre(arquivo);
        novaRaiz->ehfolha = 0; // Raiz nova com filhos deixa de ser folha
        novaRaiz->pai = -1;
        
        memcpy(novaRaiz->chave[0], chaveMediana, 100);
        novaRaiz->filho[0] = p->indice;
        novaRaiz->filho[1] = novaPagina->indice;
        novaRaiz->qtElementos = 1;

        // Atualiza os pais de p e novaPagina para apontarem para a nova raiz
        p->pai = novaRaiz->indice;
        novaPagina->pai = novaRaiz->indice;

        // Atualiza o cabeçalho da árvore no arquivo
        header.raiz = novaRaiz->indice;
        header.qtdPaginas += 2; // Criou novaPagina e novaRaiz
        fseek(arquivo, 0, SEEK_SET);
        fwrite(&header, sizeof(Cabecalho), 1, arquivo);

        // Salva a nova raiz no arquivo
        fseek(arquivo, sizeof(Cabecalho) + novaRaiz->indice * sizeof(Pagina), SEEK_SET);
        fwrite(novaRaiz, sizeof(Pagina), 1, arquivo);
        destroiPagina(novaRaiz);

    } else {
        // Se já possui pai, carrega o pai do disco e insere recursivamente
        Pagina pai;
        fseek(arquivo, sizeof(Cabecalho) + p->pai * sizeof(Pagina), SEEK_SET);
        fread(&pai, sizeof(Pagina), 1, arquivo);

        // Atualiza a contagem de páginas criadas no cabeçalho
        header.qtdPaginas++;
        fseek(arquivo, 0, SEEK_SET);
        fwrite(&header, sizeof(Cabecalho), 1, arquivo);

        // Insere a chave que subiu no pai. O "filho" associado a ela é a novaPagina.
        // Como o pai é um nó interno, burlamos temporariamente o check de folha da função de inserção
        pai.ehfolha = 1; 
        inserirElemento(&pai, chaveMediana, novaPagina->indice);
        pai.ehfolha = 0; // Devolve o status original de nó interno do pai
    }

    // Salva as alterações das páginas manipuladas de volta no arquivo
    fseek(arquivo, sizeof(Cabecalho) + p->indice * sizeof(Pagina), SEEK_SET);
    fwrite(p, sizeof(Pagina), 1, arquivo);

    fseek(arquivo, sizeof(Cabecalho) + novaPagina->indice * sizeof(Pagina), SEEK_SET);
    fwrite(novaPagina, sizeof(Pagina), 1, arquivo);

    destroiPagina(novaPagina);
}

void verificarUnderflow(FILE *arquivo, Pagina *pagina){
    Cabecalho header;

    // lê cabeçalho
    fseek(arquivo, 0, SEEK_SET);
    fread(&header, sizeof(Cabecalho), 1, arquivo);

    int minimo = ORDEM / 2;

    // não existe underflow
    if (pagina->qtElementos >= minimo)
        return;

    // caso a página seja raiz
    if (pagina->indice == header.raiz){
        // se a raiz ficou vazia, árvore fica vazia
        if (pagina->qtElementos == 0){
            header.raiz = -1;

            fseek(arquivo, 0, SEEK_SET);
            fwrite(&header, sizeof(Cabecalho), 1, arquivo);
        }
        return;
    }

    // carrega pai
    Pagina pai;

    fseek(arquivo, sizeof(Cabecalho) + pagina->pai * sizeof(Pagina), SEEK_SET);
    fread(&pai, sizeof(Pagina), 1, arquivo);

    // descobre posição da página no pai
    int pos = 0;

    while (pai.filho[pos] != pagina->indice) pos++;

    Pagina irmaAdjacente;

    // tenta pegar da irmã esquerda
    if (pos > 0){
        fseek(arquivo, sizeof(Cabecalho) + pai.filho[pos-1] * sizeof(Pagina), SEEK_SET);
        fread(&irmaAdjacente, sizeof(Pagina), 1, arquivo);

        if (irmaAdjacente.qtElementos > minimo){

            // move elementos da página para abrir espaço
            for (int i = pagina->qtElementos; i > 0; i--){
                memcpy(pagina->chave[i],
                       pagina->chave[i-1],
                       sizeof(pagina->chave[i]));

                pagina->filho[i] = pagina->filho[i-1];
            }
            // pega maior elemento do irmão esquerdo

            memcpy(pagina->chave[0],
                   irmaAdjacente.chave[irmaAdjacente.qtElementos-1],
                   sizeof(pagina->chave[0]));

            pagina->filho[0] = irmaAdjacente.filho[irmaAdjacente.qtElementos-1];
            pagina->qtElementos++;
            irmaAdjacente.qtElementos--;

            // salva a irmã

            fseek(arquivo, sizeof(Cabecalho) + irmaAdjacente.indice*sizeof(Pagina), SEEK_SET);
            fwrite(&irmaAdjacente,sizeof(Pagina),1,arquivo);

            return;
        }
    }

    //tenta pegar da irmã direita
    if (pos < pai.qtElementos){
        fseek(arquivo, sizeof(Cabecalho) + pai.filho[pos+1]*sizeof(Pagina), SEEK_SET);
        fread(&irmaAdjacente,sizeof(Pagina),1,arquivo);

        if (irmaAdjacente.qtElementos > minimo){

            memcpy(pagina->chave[pagina->qtElementos],
                   irmaAdjacente.chave[0],
                   sizeof(pagina->chave[0]));

            pagina->filho[pagina->qtElementos] = irmaAdjacente.filho[0];
            pagina->qtElementos++;

            // remove primeiro elemento da irmã
            for(int i=0;i<irmaAdjacente.qtElementos-1;i++){

                memcpy(irmaAdjacente.chave[i],
                       irmaAdjacente.chave[i+1],
                       sizeof(irmaAdjacente.chave[i]));

                irmaAdjacente.filho[i] = irmaAdjacente.filho[i+1];
            }

            irmaAdjacente.qtElementos--;

            fseek(arquivo, sizeof(Cabecalho) + irmaAdjacente.indice*sizeof(Pagina), SEEK_SET);
            fwrite(&irmaAdjacente,sizeof(Pagina),1,arquivo);

            return;
        }
    }

    //se chegou aqui, não conseguiu redistribuir
    //faz concatenação !!!

    if (pos > 0){

        fseek(arquivo, sizeof(Cabecalho) + pai.filho[pos-1]*sizeof(Pagina), SEEK_SET);
        fread(&irmaAdjacente,sizeof(Pagina),1,arquivo);

        for(int i=0;i<pagina->qtElementos;i++){

            memcpy(irmaAdjacente.chave[irmaAdjacente.qtElementos],
                   pagina->chave[i],
                   sizeof(pagina->chave[i]));

            irmaAdjacente.filho[irmaAdjacente.qtElementos] = pagina->filho[i];
            irmaAdjacente.qtElementos++;
        }

        // marca página como removida
        pagina->foiDeletada = 1;

        fseek(arquivo, sizeof(Cabecalho) + irmaAdjacente.indice*sizeof(Pagina), SEEK_SET);
        fwrite(&irmaAdjacente,sizeof(Pagina),1,arquivo);

        // remove referência no pai
        for(int i=pos;i<pai.qtElementos;i++){
            pai.filho[i] = pai.filho[i+1];
        }
        pai.qtElementos--;

        fseek(arquivo, sizeof(Cabecalho) + pai.indice*sizeof(Pagina), SEEK_SET);
        fwrite(&pai,sizeof(Pagina),1,arquivo);
    }
}

void ordenarPaginaFolha(Pagina *p){
    int j, aux2;
    unsigned char aux1[100]; 

    for (int i = 1; i < p->qtElementos; i++) {
        memcpy(aux1, p->chave[i], 100);
        aux2 = p->filho[i]; 

        // Loop do Insertion Sort
        for (j = i; j > 0 && strcmp((char*)aux1, (char*)p->chave[j - 1]) < 0; j--) {
            
            memcpy(p->chave[j], p->chave[j - 1], 100); 
            p->filho[j] = p->filho[j - 1];             
        }
        
        // Coloca os valores nos seus devidos lugares ordenados
        memcpy(p->chave[j], aux1, 100);
        p->filho[j] = aux2;
    }
}

int buscarPaginaLivre(){
    //abre o arquivo pra leitura
    FILE *arquivo = fopen(arquivoArvore, "rb+");
    if (arquivo == NULL){
        printf("Erro ao abrir o arquivo!\n");
        return;
    }
    //pula o cabecalho
    fseek(arquivo, sizeof(Cabecalho), SEEK_SET);
    //le pagina a pagina até encontrar uma que foi deletada logicamente
    Pagina p;
    int i = 0;
    while(fread(&p, sizeof(Pagina), 1, arquivo)){
        if (p.foiDeletada)
            break;
        i++;
    }
    //fecha o arquivo
    fclose(arquivo);
    //retorna indice da página já deletada
    return i;
}

// funcões para a árvore
void inicializarArvore(char* nomeArquivo, int ordem, int tamChave){
    
    FILE *arquivoArvore = fopen(ARQUIVO_ARVORE, "rb");
    // Confere se arquivo  da árvore já não foi criado
    if (arquivoArvore == NULL)
    {
        Cabecalho arvore;
        arvore.raiz = -1;
        arvore.qtdPaginas = 0;
        arvore.tamChave = tamChave;
        arvore.ordem = ordem; // podemos tirar isso e usar do define
        arvore.qtdPaginas = 0;

        arquivoArvore = fopen(ARQUIVO_ARVORE, "wb+");
        fwrite(&arvore, sizeof(Cabecalho), 1, arquivoArvore);
    }

    else 
        printf("Arquivo da árvore já existe!\n");

    fclose(arquivoArvore);

}
void imprimirArvore();

Pagina buscarFolha(FILE *arquivo, Cabecalho *header, const void *chave, int (*comparar)(const void *, const void *)){
    
    Pagina pagina;

    //Carrega a raiz
    fseek(arquivo, sizeof(Cabecalho) + header->raiz * sizeof(Pagina), SEEK_SET);
    fread(&pagina, sizeof(Pagina), 1, arquivo);

    // Enquanto não chegar em uma folha
    while (pagina.ehfolha == 0){
        int i = 0;

        // Descobre qual filho seguir
        while (i < pagina.qtElementos && comparar(chave, pagina.chave[i]) > 0) i++;
        
        // Carrega o filho escolhido
        fseek(arquivo, sizeof(Cabecalho) + pagina.filho[i] * sizeof(Pagina), SEEK_SET);
        fread(&pagina, sizeof(Pagina), 1, arquivo);
    }   
    return pagina;
}

int buscarChaveNaArvore(const void* chave, int *enderecoRegistro, int (*comparar)(const void*, const void*)){

    FILE *arquivo = fopen(arquivoArvore, "rb");

    if (arquivo == NULL){
        printf("Erro ao abrir o arquivo!\n");
        return -1;
    }

    Cabecalho header;

    if (fread(&header, sizeof(Cabecalho), 1, arquivo) != 1){
        printf("Erro ao ler o cabeçalho!!\n");
        fclose(arquivo);
        return -1;
    }


    if (header.raiz == -1){
        printf("Árvore vazia!!\n");
        fclose(arquivo);
        return -1;
    }

    // busca a folha
    Pagina p = buscarFolha(arquivo, &header, chave, comparar);

    // procura a chave na folha
    for (int i = 0; i < p.qtElementos; i++){

        if (comparar(chave, p.chave[i]) == 0){

            *enderecoRegistro = p.filho[i];

            fclose(arquivo);
            return 1;
        }
    }


    fclose(arquivo);

    return -1;
}

void inserirChaveNaArvore(const void *chave, int enderecoRegistro, size_t tamanhoChave, int (*comparar)(const void *, const void *)){

    FILE *arquivo = fopen(arquivoArvore, "rb+");

    if (arquivo == NULL){
        printf("Erro ao abrir o arquivo!\n");
        return;
    }

    Cabecalho header;
    fread(&header, sizeof(Cabecalho), 1, arquivo);

    // Árvore vazia
    if (header.raiz == -1){

        Pagina *raiz = criaPagina();
        inicializarPagina(raiz, 0, 1);
        inserirElemento(raiz, chave);
        raiz->filho[0] = enderecoRegistro;
        header.raiz = 0;
        header.qtdPaginas = 1;

        fseek(arquivo, 0, SEEK_SET);
        fwrite(&header, sizeof(Cabecalho), 1, arquivo);

        fseek(arquivo, sizeof(Cabecalho), SEEK_SET);
        fwrite(raiz, sizeof(Pagina), 1, arquivo);

        destroiPagina(raiz);

        fclose(arquivo);
        return;
    }

    //encontra a folha
    Pagina pagina = buscarFolha(arquivo, &header, chave, comparar);

    //insere na folha
    inserirElemento(&pagina, chave);
    pagina.filho[pagina.qtElementos - 1] = enderecoRegistro;
    ordenarPaginaFolha(&pagina);
    verificarOverflow(&pagina);

    //salva a página
    fseek(arquivo, sizeof(Cabecalho) + pagina.indice * sizeof(Pagina), SEEK_SET);
    fwrite(&pagina, sizeof(Pagina), 1, arquivo);
    fclose(arquivo);
}

void deletarChaveNaArvore(const void *chave, int (*comparar)(const void *, const void *)){

    FILE *arquivo = fopen(arquivoArvore, "rb+");

    if (arquivo == NULL){
        printf("Erro ao abrir o arquivo!!!\n");
        return;
    }

    Cabecalho header;

    if (fread(&header, sizeof(Cabecalho), 1, arquivo) != 1){
        fclose(arquivo);
        return;
    }

    if (header.raiz == -1){
        printf("Árvore vazia!\n");
        fclose(arquivo);
        return;
    }

    // encontra a folha
    Pagina pagina = buscarFolha(arquivo, &header, chave, comparar);

    int encontrou = 0;

    for (int i = 0; i < pagina.qtElementos; i++){
        if (comparar(chave, pagina.chave[i]) == 0){
            encontrou = 1;
            break;
        }
    }

    if (!encontrou){
        printf("Chave não encontrada.\n");
        fclose(arquivo);
        return;
    }

    removerElemento(&pagina, chave);
    verificarUnderflow(arquivo, &pagina);

    fseek(arquivo, sizeof(Cabecalho) + pagina.indice * sizeof(Pagina), SEEK_SET);

    fwrite(&pagina, sizeof(Pagina), 1, arquivo);
    fclose(arquivo);
}


