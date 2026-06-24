#ifndef BPLUS_H
#define BPLUS_H
#include <stdio.h>
#include <stdlib.h>

#define P 3
#define PFOLHA 2     

typedef struct No {
    int folha;
    int n;

    void* chaves[P];       
    void* registros[P]; 
    
    struct No* filhos[P + 1];
    struct No* prox;
} No;

typedef struct {
    No* raiz;
    
    int (*comparar)(void* chave1, void* chave2); 
    void (*imprimir)(void* chave, void* registro);
    
    size_t (*tamanho_chave)();
    size_t (*tamanho_registro)();
    void (*gravar_disco)(void* registro, FILE* arquivo);
    void* (*ler_disco)(FILE* arquivo);

} ArvoreBPlus;

ArvoreBPlus* criarArvore(
    int (*compara)(void*, void*), 
    void (*imprime)(void*, void*),
    size_t (*tamanho_c)(),
    size_t (*tamanho_r)(),
    void (*gravar)(void*, FILE*),
    void* (*ler)(FILE*)
);

No* criarNo(int dado);
int encontrarPosicao(ArvoreBPlus* arvore, No* no, void* chave);
// ... atualizar o restante para receber 'ArvoreBPlus* arvore' e 'void* chave' ...

No* criarNo(int folha);

int encontrarPosicao(No* no, int chave);

No* buscarFolha(No* raiz, int chave);

int buscar(No* raiz, int chave);

Resultado inserirEmFolha(No* folha, int chave);

Resultado inserirRec(No* no, int chave);

Resultado inserirEmInterno(No* no, int chave, No* novoFilho);

No* inserir(No* raiz, int chave);

int validarArvore(No* raiz);

int validarArvoreRec(No* no, int ehRaiz);

void buscarIntervalo(No* raiz, int inicio, int fim);


int removerDaFolha(No* folha, int chave);

ResultadoRemocao removerRec(No* no, int chave);

int corrigirUnderflow(No* pai, int indiceFilho);

void remover(No** raiz, int chave);

void imprimirArvore(No* raiz);

#endif