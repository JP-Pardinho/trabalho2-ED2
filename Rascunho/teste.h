#ifndef Bplus_H

#define ORDEM 4
#include <stdbool.h>

typedef struct {
    int ordem, qtdPaginas, raiz; //raiz recebe -1 na criação do cabeçalho == VAZIA
    size_t tamChave;
} Cabecalho;

typedef struct Pagina{
    void* chave[ORDEM + 1];
    int filho[ORDEM + 2];
    int pai; //pai do vetor de Chave
    int indice; //endereço da página
    int proximaFolha; //endereço irma adjacente
    int qtElementos; //qt elementos ocupados 
    int ehfolha; // 0 == pag interna e 1 == pag folha
    int foiDeletada; //marcador lógico
} Pagina;

// funções para a página
Pagina *criaPagina(); // eduardo
void inicializarPagina(Pagina *pagina, int indice, int tipo); // eduardo
void destroiPagina(Pagina *p); // eduardo
void inserirElemento(Pagina *p, const void *chave, int indice, int (*comparar)(const void *, const void *)); // eduardo
int removerElemento(Pagina *p, const void *chave, int (*comparar)(const void *, const void *));
void verificarOverflow(Pagina *p, int (*comparar)(const void *, const void *) ); // eduardo
void verificarUnderflow(Pagina *pagina);
void ordenarPaginaFolha(Pagina *p, int (*comparar)(const void *, const void *)); // eduardo
int buscarPaginaLivre();

// funções para a árvore
void inicializarArvore(int ordem, int tamChave, int (*comparar)(const void*, const void*));
void imprimirArvore();
int buscarChave(const void *chave, int (*comparar)(const void*, const void*));
void inserirChave(const void *chave, int enderecoRegistro);
void deletarChave(const void *chave);
int* buscarChavesIntervalo(const void *chave_min, const void *chave_max, int *qtEncontrados, int (*comparar)(const void*, const void*));

#endif