#include "Bplus.h"

// Estrutura interna para salvar no offset 0 do arquivo
typedef struct {
    long offset_raiz;         // Onde a raiz está armazenada
    size_t tamanho_chave;     // Tamanho da chave em bytes
    size_t tamanho_registro;  // Tamanho do registro em bytes
} CabecalhoArquivo;

// Estrutura auxiliar para a subida recursiva (deve ir para o topo do Bplus.c)
typedef struct {
    bool promoveu;
    long offset_novo_no;
    void *chave_promovida; // Ponteiro genérico para a chave que subiu
} ResultadoInsercao;

// =======================================================
// Lógica de Leitura Física (Disco -> RAM)
// =======================================================
void ler_no(ArvoreBPlus *arvore, long offset, CabecalhoNo *cabecalho, void *chaves, void *dados, long *filhos) {
    // 1. Posiciona o "leitor" do arquivo no byte exato (offset) a partir do início (SEEK_SET)
    fseek(arvore->arquivo, offset, SEEK_SET);

    // 2. Lê o cabeçalho (que tem tamanho fixo)
    fread(cabecalho, sizeof(CabecalhoNo), 1, arvore->arquivo);

    // 3. Lê os blocos genéricos de chaves e dados
    // Lemos 'P' elementos do tamanho que foi configurado na árvore
    fread(chaves, arvore->tamanho_chave, P, arvore->arquivo);
    
    // Lê os dados (registros). Se for um nó interno, isso pode ser lixo de memória, 
    // mas lemos mesmo assim para manter a simetria do bloco no disco.
    fread(dados, arvore->tamanho_registro, P, arvore->arquivo);

    // 4. Lê os ponteiros (offsets) para os filhos
    fread(filhos, sizeof(long), P + 1, arvore->arquivo);
}

// =======================================================
// Lógica de Escrita Física (RAM -> Disco)
// =======================================================
void gravar_no(ArvoreBPlus *arvore, long offset, CabecalhoNo *cabecalho, void *chaves, void *dados, long *filhos) {
    // 1. Posiciona o "gravador" do arquivo no byte exato
    fseek(arvore->arquivo, offset, SEEK_SET);

    // 2. Grava o cabeçalho
    fwrite(cabecalho, sizeof(CabecalhoNo), 1, arvore->arquivo);

    // 3. Grava as chaves e os dados
    fwrite(chaves, arvore->tamanho_chave, P, arvore->arquivo);
    fwrite(dados, arvore->tamanho_registro, P, arvore->arquivo);

    // 4. Grava os offsets dos filhos
    fwrite(filhos, sizeof(long), P + 1, arvore->arquivo);

    // 5. Força o sistema operacional a descarregar o buffer pro disco físico imediatamente
    fflush(arvore->arquivo);
}

ArvoreBPlus* criar_arvore(const char *nome_arquivo, size_t tam_chave, size_t tam_registro, ComparaChaveFunc func_compara) {
    ArvoreBPlus *arvore = (ArvoreBPlus*) malloc(sizeof(ArvoreBPlus));
    if (!arvore) return NULL;

    arvore->tamanho_chave = tam_chave;
    arvore->tamanho_registro = tam_registro;
    arvore->compara = func_compara;

    // 1. Tenta abrir o arquivo no modo de leitura e escrita binária ("rb+")
    // Este modo exige que o arquivo já exista.
    arvore->arquivo = fopen(nome_arquivo, "rb+");

    if (arvore->arquivo == NULL) {
        // Se retornou NULL, o arquivo não existe. Vamos criá-lo do zero com "wb+"
        arvore->arquivo = fopen(nome_arquivo, "wb+");
        if (arvore->arquivo == NULL) {
            free(arvore);
            return NULL;
        }

        // Como o arquivo é novo, a árvore está vazia. A raiz inicial não existe (-1).
        arvore->offset_raiz = -1;

        // Cria o cabeçalho do arquivo para salvar no offset 0
        CabecalhoArquivo cabecalho_file;
        cabecalho_file.offset_raiz = arvore->offset_raiz;
        cabecalho_file.tamanho_chave = arvore->tamanho_chave;
        cabecalho_file.tamanho_registro = arvore->tamanho_registro;

        // Grava os metadados no início do arquivo
        fseek(arvore->arquivo, 0, SEEK_SET);
        fwrite(&cabecalho_file, sizeof(CabecalhoArquivo), 1, arvore->arquivo);
        fflush(arvore->arquivo);
    } else {
        // Se o arquivo já existia, precisamos ler os metadados salvos anteriormente
        CabecalhoArquivo cabecalho_file;
        fseek(arvore->arquivo, 0, SEEK_SET);
        fread(&cabecalho_file, sizeof(CabecalhoArquivo), 1, arvore->arquivo);

        // Restaura o estado da árvore a partir do disco
        arvore->offset_raiz = cabecalho_file.offset_raiz;
        // Opcional: Validar se os tamanhos passados coincidem com os do arquivo
        arvore->tamanho_chave = cabecalho_file.tamanho_chave;
        arvore->tamanho_registro = cabecalho_file.tamanho_registro;
    }

    return arvore;
}

// =======================================================
// Fechamento Seguro da Árvore
// =======================================================
void fechar_arvore(ArvoreBPlus *arvore) {
    if (arvore == NULL) return;

    if (arvore->arquivo != NULL) {
        // Antes de fechar, atualiza o offset da raiz no início do arquivo
        // garantindo que qualquer alteração de raiz foi salva.
        CabecalhoArquivo cabecalho_file;
        cabecalho_file.offset_raiz = arvore->offset_raiz;
        cabecalho_file.tamanho_chave = arvore->tamanho_chave;
        cabecalho_file.tamanho_registro = arvore->tamanho_registro;

        fseek(arvore->arquivo, 0, SEEK_SET);
        fwrite(&cabecalho_file, sizeof(CabecalhoArquivo), 1, arvore->arquivo);
        fflush(arvore->arquivo);

        fclose(arvore->arquivo);
    }

    free(arvore);
}

// =======================================================
// Alocação de Espaço Físico para um Novo Nó
// =======================================================
long alocar_novo_no(ArvoreBPlus *arvore, int eh_folha) {
    // 1. Move o ponteiro para o fim do ficheiro para determinar o novo offset
    fseek(arvore->arquivo, 0, SEEK_END);
    long offset_novo = ftell(arvore->arquivo);

    // 2. Inicializa o cabeçalho do nó
    CabecalhoNo cabecalho;
    cabecalho.folha = eh_folha;
    cabecalho.n = 0;
    cabecalho.offset_pai = -1;
    cabecalho.offset_prox_folha = -1;

    // 3. Aloca buffers temporários na RAM zerados para criar a estrutura no disco
    void *chaves = calloc(P, arvore->tamanho_chave);
    void *dados = calloc(P, arvore->tamanho_registro);
    long *filhos = (long*) calloc(P + 1, sizeof(long));

    // Inicializa os ponteiros de filhos com -1 (nulo em disco)
    for (int i = 0; i <= P; i++) {
        filhos[i] = -1;
    }

    // 4. Grava o nó estruturado e vazio no final do ficheiro
    gravar_no(arvore, offset_novo, &cabecalho, chaves, dados, filhos);

    // 5. Liberta a memória RAM temporária de paginação
    free(chaves);
    free(dados);
    free(filhos);

    return offset_novo;
}

// =======================================================
// Pesquisa Genérica por Igualdade
// =======================================================
bool buscar_registro(ArvoreBPlus *arvore, void *chave, void *registro_retorno) {
    // Se a árvore estiver vazia, a busca falha imediatamente
    if (arvore->offset_raiz == -1) {
        return false;
    }

    long offset_atual = arvore->offset_raiz;

    // Alocação de memória na RAM estritamente como área de paginação temporária 
    CabecalhoNo cabecalho;
    void *chaves = malloc(P * arvore->tamanho_chave);
    void *dados = malloc(P * arvore->tamanho_registro);
    long *filhos = (long*) malloc((P + 1) * sizeof(long));

    bool encontrado = false;

    // Desce na estrutura da árvore indexada até atingir um nó folha
    while (offset_atual != -1) {
        ler_no(arvore, offset_atual, &cabecalho, chaves, dados, filhos);

        if (cabecalho.folha) {
            // Percorre as chaves do nó folha à procura de uma correspondência exata
            for (int i = 0; i < cabecalho.n; i++) {
                // Calcula o endereço de memória da chave atual baseado no offset de bytes
                void *chave_atual = (char*)chaves + (i * arvore->tamanho_chave);

                // Utiliza o callback para fazer a comparação genérica [cite: 70]
                if (arvore->compara(chave, chave_atual) == 0) {
                    // Copia o registo correspondente encontrado para o parâmetro de retorno
                    void *dado_encontrado = (char*)dados + (i * arvore->tamanho_registro);
                    memcpy(registro_retorno, dado_encontrado, arvore->tamanho_registro);
                    encontrado = true;
                    break;
                }
            }
            // Finaliza a pesquisa pois chegou ao nível folha
            break; 
        } else {
            // Nó Interno: Encontra o ponteiro (filho) correto para descer
            int pos = 0;
            while (pos < cabecalho.n) {
                void *chave_atual = (char*)chaves + (pos * arvore->tamanho_chave);
                
                if (arvore->compara(chave, chave_atual) >= 0) {
                    pos++;
                } else {
                    break;
                }
            }
            // Atualiza o offset para carregar o nó do próximo nível na próxima iteração
            offset_atual = filhos[pos];
        }
    }

    // Liberta a página da RAM antes de retornar o resultado 
    free(chaves);
    free(dados);
    free(filhos);

    return encontrado;
}

// =======================================================
// Inserção em Nó Folha com tratamento de Overflow (Cisão)
// =======================================================
ResultadoInsercao inserir_em_folha(ArvoreBPlus *arvore, long offset_folha, void *chave_nova, void *registro_novo) {
    ResultadoInsercao res;
    res.promoveu = false;
    res.offset_novo_no = -1;
    // Aloca o buffer para guardar a cópia da chave que subirá (se houver split)
    res.chave_promovida = malloc(arvore->tamanho_chave); 

    CabecalhoNo cabecalho;
    
    // TRUQUE: Alocamos PFOLHA + 1 na RAM para fazer o overflow temporário de forma limpa!
    void *chaves = malloc((PFOLHA + 1) * arvore->tamanho_chave); 
    void *dados = malloc((PFOLHA + 1) * arvore->tamanho_registro);
    long *filhos = malloc((P + 1) * sizeof(long));

    // 1. Carrega o nó folha do disco para a RAM
    ler_no(arvore, offset_folha, &cabecalho, chaves, dados, filhos);

    // 2. Encontra a posição correta mantendo a ordenação (Insertion Sort genérico)
    int pos = cabecalho.n - 1;
    while (pos >= 0) {
        void *chave_atual = (char*)chaves + (pos * arvore->tamanho_chave);
        
        // Se a chave atual for MAIOR que a nova, arrasta atual para a direita
        if (arvore->compara(chave_atual, chave_nova) > 0) {
            void *dest_chave = (char*)chaves + ((pos + 1) * arvore->tamanho_chave);
            memcpy(dest_chave, chave_atual, arvore->tamanho_chave);

            void *dado_atual = (char*)dados + (pos * arvore->tamanho_registro);
            void *dest_dado = (char*)dados + ((pos + 1) * arvore->tamanho_registro);
            memcpy(dest_dado, dado_atual, arvore->tamanho_registro);
            
            pos--;
        } else {
            break; // Achou a posição!
        }
    }
    
    // 3. Insere a nova chave e o novo dado na posição encontrada
    pos++;
    memcpy((char*)chaves + (pos * arvore->tamanho_chave), chave_nova, arvore->tamanho_chave);
    memcpy((char*)dados + (pos * arvore->tamanho_registro), registro_novo, arvore->tamanho_registro);
    cabecalho.n++;

    // 4. VERIFICAÇÃO DE OVERFLOW
    if (cabecalho.n <= PFOLHA) {
        // Sem overflow. Apenas sobrescreve o nó atual no disco com o dado novo.
        gravar_no(arvore, offset_folha, &cabecalho, chaves, dados, filhos);
        free(chaves); free(dados); free(filhos);
        return res;
    }

    // 5. OCORREU OVERFLOW: HORA DO CISALHAMENTO (SPLIT)
    int meio = cabecalho.n / 2;
    
    // Cria fisicamente um novo nó folha no final do arquivo e pega o offset dele
    long offset_nova_folha = alocar_novo_no(arvore, 1);
    
    CabecalhoNo cab_nova_folha;
    void *chaves_nova = malloc(PFOLHA * arvore->tamanho_chave);
    void *dados_nova = malloc(PFOLHA * arvore->tamanho_registro);
    long *filhos_nova = malloc((P + 1) * sizeof(long));
    ler_no(arvore, offset_nova_folha, &cab_nova_folha, chaves_nova, dados_nova, filhos_nova);

    // 6. Transfere a metade direita dos dados para a nova folha
    int n_transferidos = cabecalho.n - meio;
    for (int i = 0; i < n_transferidos; i++) {
        void *origem_chave = (char*)chaves + ((meio + i) * arvore->tamanho_chave);
        void *dest_chave = (char*)chaves_nova + (i * arvore->tamanho_chave);
        memcpy(dest_chave, origem_chave, arvore->tamanho_chave);

        void *origem_dado = (char*)dados + ((meio + i) * arvore->tamanho_registro);
        void *dest_dado = (char*)dados_nova + (i * arvore->tamanho_registro);
        memcpy(dest_dado, origem_dado, arvore->tamanho_registro);
    }

    // Atualiza as contagens
    cab_nova_folha.n = n_transferidos;
    cabecalho.n = meio;

    // 7. Reajusta a Lista Encadeada das Folhas (Para a Busca por Intervalo)
    cab_nova_folha.offset_prox_folha = cabecalho.offset_prox_folha;
    cabecalho.offset_prox_folha = offset_nova_folha;

    // 8. Define o pacote de promoção para o Pai
    // Em árvores B+, a folha promove uma CÓPIA do seu primeiro elemento novo
    memcpy(res.chave_promovida, chaves_nova, arvore->tamanho_chave);
    res.promoveu = true;
    res.offset_novo_no = offset_nova_folha;

    // 9. Salva o estado atualizado dos DOIS nós no disco
    gravar_no(arvore, offset_folha, &cabecalho, chaves, dados, filhos);
    gravar_no(arvore, offset_nova_folha, &cab_nova_folha, chaves_nova, dados_nova, filhos_nova);

    // Libera a área temporária da RAM
    free(chaves); free(dados); free(filhos);
    free(chaves_nova); free(dados_nova); free(filhos_nova);

    return res;
}

// =======================================================
// Inserção em Nó Interno com tratamento de Overflow (Cisão)
// =======================================================
ResultadoInsercao inserir_em_interno(ArvoreBPlus *arvore, long offset_interno, void *chave_promovida_filho, long offset_filho_dir) {
    ResultadoInsercao res;
    res.promoveu = false;
    res.offset_novo_no = -1;
    res.chave_promovida = malloc(arvore->tamanho_chave);

    CabecalhoNo cabecalho;
    // Espaço estendido na RAM (P chaves e P+1 filhos) para overflow temporário
    void *chaves = malloc((P) * arvore->tamanho_chave);
    void *dados = malloc((P) * arvore->tamanho_registro); // Nós internos não usam dados, mas mantemos o alinhamento
    long *filhos = malloc((P + 2) * sizeof(long));

    ler_no(arvore, offset_interno, &cabecalho, chaves, dados, filhos);

    // 1. Encontra a posição correta para inserir a chave que subiu do filho
    int pos = cabecalho.n - 1;
    while (pos >= 0) {
        void *chave_atual = (char*)chaves + (pos * arvore->tamanho_chave);
        
        if (arvore->compara(chave_atual, chave_promovida_filho) > 0) {
            memcpy((char*)chaves + ((pos + 1) * arvore->tamanho_chave), chave_atual, arvore->tamanho_chave);
            filhos[pos + 2] = filhos[pos + 1];
            pos--;
        } else {
            break;
        }
    }
    
    pos++;
    memcpy((char*)chaves + (pos * arvore->tamanho_chave), chave_promovida_filho, arvore->tamanho_chave);
    filhos[pos + 1] = offset_filho_dir;
    cabecalho.n++;

    // 2. Verifica se o nó interno estourou o limite legal (P - 1 chaves)
    if (cabecalho.n <= P - 1) {
        gravar_no(arvore, offset_interno, &cabecalho, chaves, dados, filhos);
        free(chaves); free(dados); free(filhos);
        return res;
    }

    // 3. OCORREU OVERFLOW INTERNO: HORA DO SPLIT
    int meio = cabecalho.n / 2; // Índice da chave que vai subir para o pai
    
    // Aloca um novo nó interno no arquivo
    long offset_novo_interno = alocar_novo_no(arvore, 0);
    
    CabecalhoNo cab_novo;
    void *chaves_novo = malloc(P * arvore->tamanho_chave);
    void *dados_novo = malloc(P * arvore->tamanho_registro);
    long *filhos_novo = malloc((P + 1) * sizeof(long));
    ler_no(arvore, offset_novo_interno, &cab_novo, chaves_novo, dados_novo, filhos_novo);

    // A chave do meio é salva para ser promovida
    memcpy(res.chave_promovida, (char*)chaves + (meio * arvore->tamanho_chave), arvore->tamanho_chave);

    // 4. Move os elementos da direita (após o meio) para o novo nó interno
    int j = 0;
    for (int i = meio + 1; i < cabecalho.n; i++) {
        memcpy((char*)chaves_novo + (j * arvore->tamanho_chave), (char*)chaves + (i * arvore->tamanho_chave), arvore->tamanho_chave);
        filhos_novo[j] = filhos[i];
        j++;
    }
    filhos_novo[j] = filhos[cabecalho.n]; // Move o último ponteiro de filho

    cab_novo.n = j;
    cabecalho.n = meio; // O nó antigo perde a metade direita e a própria chave do meio

    cab_novo.offset_pai = cabecalho.offset_pai;
    res.promoveu = true;
    res.offset_novo_no = offset_novo_interno;

    // 5. Salva os nós modificados no arquivo
    gravar_no(arvore, offset_interno, &cabecalho, chaves, dados, filhos);
    gravar_no(arvore, offset_novo_interno, &cab_novo, chaves_novo, dados_novo, filhos_novo);

    // 6. ATUALIZAÇÃO DOS PONTEIROS DE PAI NO DISCO
    // Como mudamos os filhos de lugar, precisamos avisar a esses filhos quem é o novo pai deles
    for (int i = 0; i <= cab_novo.n; i++) {
        long off_filho = filhos_novo[i];
        if (off_filho != -1) {
            CabecalhoNo cab_filho;
            void *ch_f = malloc(arvore->tamanho_chave * P);
            void *d_f = malloc(arvore->tamanho_registro * P);
            long *fi_f = malloc(sizeof(long) * (P + 1));
            
            ler_no(arvore, off_filho, &cab_filho, ch_f, d_f, fi_f);
            cab_filho.offset_pai = offset_novo_interno;
            gravar_no(arvore, off_filho, &cab_filho, ch_f, d_f, fi_f);
            
            free(ch_f); free(d_f); free(fi_f);
        }
    }

    free(chaves); free(dados); free(filhos);
    free(chaves_novo); free(dados_novo); free(filhos_novo);

    return res;
}

// Protótipo interno da função recursiva
ResultadoInsercao inserir_rec(ArvoreBPlus *arvore, long offset_atual, void *chave, void *registro);

ResultadoInsercao inserir_rec(ArvoreBPlus *arvore, long offset_atual, void *chave, void *registro) {
    CabecalhoNo cabecalho;
    // Buffers temporários apenas para ler o nó e decidir o caminho
    void *chaves = malloc(P * arvore->tamanho_chave);
    void *dados = malloc(P * arvore->tamanho_registro);
    long *filhos = malloc((P + 1) * sizeof(long));

    ler_no(arvore, offset_atual, &cabecalho, chaves, dados, filhos);

    // Se alcançou uma folha, delega a inserção física nela
    if (cabecalho.folha) {
        free(chaves); free(dados); free(filhos);
        return inserir_em_folha(arvore, offset_atual, chave, registro);
    }

    // Se for nó interno, encontra o ponteiro de disco correto para descer
    int pos = 0;
    while (pos < cabecalho.n) {
        void *chave_atual = (char*)chaves + (pos * arvore->tamanho_chave);
        if (arvore->compara(chave, chave_atual) >= 0) {
            pos++;
        } else {
            break;
        }
    }

    long offset_filho = filhos[pos];
    free(chaves); free(dados); free(filhos);

    // Desce recursivamente para o próximo nível do arquivo
    ResultadoInsercao res_filho = inserir_rec(arvore, offset_filho, chave, registro);

    // Se o filho não dividiu, apenas repassa o resultado para cima terminando o fluxo
    if (!res_filho.promoveu) {
        return res_filho;
    }

    // Se o filho dividiu, precisamos inserir a chave promovida no nó interno atual
    ResultadoInsercao res_atual = inserir_em_interno(arvore, offset_atual, res_filho.chave_promovida, res_filho.offset_novo_no);
    free(res_filho.chave_promovida);
    
    return res_atual;
}

// =======================================================
// Interface Pública de Inserção da Árvore B+
// =======================================================
bool inserir_registro(ArvoreBPlus *arvore, void *chave, void *registro) {
    // CASO 1: Árvore totalmente vazia (Criação da primeira raiz)
    if (arvore->offset_raiz == -1) {
        long offset_raiz = alocar_novo_no(arvore, 1); // Aloca uma folha
        
        CabecalhoNo cabecalho;
        void *chaves = malloc(P * arvore->tamanho_chave);
        void *dados = malloc(P * arvore->tamanho_registro);
        long *filhos = malloc((P + 1) * sizeof(long));
        
        ler_no(arvore, offset_raiz, &cabecalho, chaves, dados, filhos);
        
        // Insere o primeiro elemento no início dos blocos de memória
        memcpy(chaves, chave, arvore->tamanho_chave);
        memcpy(dados, registro, arvore->tamanho_registro);
        cabecalho.n = 1;
        
        gravar_no(arvore, offset_raiz, &cabecalho, chaves, dados, filhos);
        arvore->offset_raiz = offset_raiz; // Atualiza o controle na RAM
        
        free(chaves); free(dados); free(filhos);
        return true;
    }

    // CASO 2: A árvore já possui nós. Dispara a recursão de disco.
    ResultadoInsercao res = inserir_rec(arvore, arvore->offset_raiz, chave, registro);

    // Se a raiz antiga sofreu divisão, a árvore precisa crescer um nível para cima
    if (res.promoveu) {
        long offset_nova_raiz = alocar_novo_no(arvore, 0); // Nova raiz sempre interna
        
        CabecalhoNo cab_raiz;
        void *chaves_raiz = malloc(P * arvore->tamanho_chave);
        void *dados_raiz = malloc(P * arvore->tamanho_registro);
        long *filhos_raiz = malloc((P + 1) * sizeof(long));
        ler_no(arvore, offset_nova_raiz, &cab_raiz, chaves_raiz, dados_raiz, filhos_raiz);

        cab_raiz.n = 1;
        memcpy(chaves_raiz, res.chave_promovida, arvore->tamanho_chave);
        filhos_raiz[0] = arvore->offset_raiz; // Filho esquerdo é a raiz antiga
        filhos_raiz[1] = res.offset_novo_no;  // Filho direito é o nó gerado no split

        gravar_no(arvore, offset_nova_raiz, &cab_raiz, chaves_raiz, dados_raiz, filhos_raiz);

        // Atualiza o ponteiro de pai nos dois nós filhos que agora estão abaixo da nova raiz
        long offsets_filhos[2] = {arvore->offset_raiz, res.offset_novo_no};
        for (int i = 0; i < 2; i++) {
            CabecalhoNo cab_f;
            void *ch_f = malloc(arvore->tamanho_chave * P);
            void *d_f = malloc(arvore->tamanho_registro * P);
            long *fi_f = malloc(sizeof(long) * (P + 1));
            
            ler_no(arvore, offsets_filhos[i], &cab_f, ch_f, d_f, fi_f);
            cab_f.offset_pai = offset_nova_raiz;
            gravar_no(arvore, offsets_filhos[i], &cab_f, ch_f, d_f, fi_f);
            
            free(ch_f); free(d_f); free(fi_f);
        }

        // Aponta o controle da árvore para a nova raiz
        arvore->offset_raiz = offset_nova_raiz;
        free(res.chave_promovida);
        free(chaves_raiz); free(dados_raiz); free(filhos_raiz);
    }

    return true;
}

// =======================================================
// Busca por Intervalo (Lista Encadeada de Folhas)
// =======================================================
void buscar_intervalo(ArvoreBPlus *arvore, void *chave_inicio, void *chave_fim, ProcessaRegistroFunc processar) {
    if (arvore->offset_raiz == -1) {
        return; // Árvore vazia, nada a buscar
    }

    long offset_atual = arvore->offset_raiz;
    CabecalhoNo cabecalho;
    
    // Aloca a página temporária na RAM
    void *chaves = malloc(P * arvore->tamanho_chave);
    void *dados = malloc(P * arvore->tamanho_registro);
    long *filhos = malloc((P + 1) * sizeof(long));

    // 1. NAVEGAÇÃO VERTICAL: Desce na árvore até achar a folha do limite inferior
    while (offset_atual != -1) {
        ler_no(arvore, offset_atual, &cabecalho, chaves, dados, filhos);
        
        if (cabecalho.folha) {
            break; // Chegamos no nível das folhas!
        } else {
            int pos = 0;
            while (pos < cabecalho.n) {
                void *chave_atual = (char*)chaves + (pos * arvore->tamanho_chave);
                // Se a chave buscada é maior ou igual, vai para a direita
                if (arvore->compara(chave_inicio, chave_atual) >= 0) {
                    pos++;
                } else {
                    break;
                }
            }
            offset_atual = filhos[pos];
        }
    }

    // 2. NAVEGAÇÃO HORIZONTAL: Varredura linear pelas folhas
    bool passou_do_fim = false;
    
    while (offset_atual != -1 && !passou_do_fim) {
        // O nó atual já foi lido. Para as iterações seguintes do while, ele lê o próximo.
        ler_no(arvore, offset_atual, &cabecalho, chaves, dados, filhos);
        
        for (int i = 0; i < cabecalho.n; i++) {
            void *chave_atual = (char*)chaves + (i * arvore->tamanho_chave);
            
            // Compara a chave lida do disco com os limites A e B
            int comp_inicio = arvore->compara(chave_atual, chave_inicio);
            int comp_fim = arvore->compara(chave_atual, chave_fim);
            
            // O requisito pede intervalo ABERTO (Nome A, Nome B)
            // Ou seja: atual > inicio E atual < fim
            if (comp_inicio > 0 && comp_fim < 0) {
                // A chave está dentro do intervalo! 
                // Calcula o endereço do registro e envia para o callback processar (imprimir)
                void *dado_atual = (char*)dados + (i * arvore->tamanho_registro);
                processar(chave_atual, dado_atual);
            } 
            else if (comp_fim >= 0) {
                // Como as chaves estão ordenadas, se a chave atual for maior ou igual
                // ao limite superior (Nome B), podemos abortar totalmente a busca.
                passou_do_fim = true;
                break;
            }
        }
        
        // Pula para o próximo bloco físico no HD (A mágica da Árvore B+)
        offset_atual = cabecalho.offset_prox_folha;
    }

    // Libera a memória de paginação
    free(chaves);
    free(dados);
    free(filhos);
}

// =======================================================
// Impressão Hierárquica da Árvore B+ em Disco
// =======================================================
void imprimir_arvore(ArvoreBPlus *arvore, ImprimeChaveFunc imprime_chave) {
    if (arvore->offset_raiz == -1) {
        printf("A arvore esta vazia no disco.\n");
        return;
    }

    // Fila simples para armazenar os offsets de cada nível
    long fila[1000]; 
    int inicio = 0, fim = 0;
    
    fila[fim++] = arvore->offset_raiz;

    CabecalhoNo cabecalho;
    void *chaves = malloc(P * arvore->tamanho_chave);
    void *dados = malloc(P * arvore->tamanho_registro);
    long *filhos = malloc((P + 1) * sizeof(long));

    int nivel = 0;

    while (inicio < fim) {
        int tamanho_nivel = fim - inicio;
        printf("Nivel %d: ", nivel);

        for (int i = 0; i < tamanho_nivel; i++) {
            long offset_atual = fila[inicio++];
            ler_no(arvore, offset_atual, &cabecalho, chaves, dados, filhos);

            printf("[ ");
            for (int j = 0; j < cabecalho.n; j++) {
                void *chave_atual = (char*)chaves + (j * arvore->tamanho_chave);
                
                // O callback injetado decide como fazer o printf dessa chave
                imprime_chave(chave_atual); 
                
                if (j < cabecalho.n - 1) printf(" | ");
            }
            printf(" ]  ");

            // Se não for folha, enfileira os offsets dos filhos para o próximo nível
            if (!cabecalho.folha) {
                for (int j = 0; j <= cabecalho.n; j++) {
                    if (filhos[j] != -1) {
                        fila[fim++] = filhos[j];
                    }
                }
            }
        }
        printf("\n");
        nivel++;
    }

    free(chaves); free(dados); free(filhos);
}

// =======================================================
// Remoção de Registro em Disco
// =======================================================
// =======================================================
// Remoção de Registro em Disco (Atualizada com Underflow)
// =======================================================
bool remover_registro(ArvoreBPlus *arvore, void *chave) {
    if (arvore->offset_raiz == -1) return false;

    long offset_atual = arvore->offset_raiz;
    long caminho_pais[100]; // Pilha para guardar a rota de descida
    int topo = 0;

    CabecalhoNo cabecalho;
    void *chaves = malloc(P * arvore->tamanho_chave);
    void *dados = malloc(P * arvore->tamanho_registro);
    long *filhos = malloc((P + 1) * sizeof(long));

    // 1. Desce na árvore até encontrar a folha correspondente
    while (true) {
        ler_no(arvore, offset_atual, &cabecalho, chaves, dados, filhos);
        
        if (cabecalho.folha) break;
        
        caminho_pais[topo++] = offset_atual; // Empilha o offset do pai
        
        int pos = 0;
        while (pos < cabecalho.n) {
            void *chave_atual = (char*)chaves + (pos * arvore->tamanho_chave);
            if (arvore->compara(chave, chave_atual) >= 0) {
                pos++;
            } else {
                break;
            }
        }
        offset_atual = filhos[pos];
    }

    // 2. Procura a chave exata dentro da folha
    int pos_remocao = -1;
    for (int i = 0; i < cabecalho.n; i++) {
        void *chave_atual = (char*)chaves + (i * arvore->tamanho_chave);
        if (arvore->compara(chave, chave_atual) == 0) {
            pos_remocao = i;
            break;
        }
    }

    // Se não encontrou a chave na folha, aborta a remoção
    if (pos_remocao == -1) {
        free(chaves); free(dados); free(filhos);
        return false; 
    }

    // 3. Remove arrastando os elementos da direita para a esquerda
    for (int i = pos_remocao; i < cabecalho.n - 1; i++) {
        void *dest_chave = (char*)chaves + (i * arvore->tamanho_chave);
        void *orig_chave = (char*)chaves + ((i + 1) * arvore->tamanho_chave);
        memcpy(dest_chave, orig_chave, arvore->tamanho_chave);

        void *dest_dado = (char*)dados + (i * arvore->tamanho_registro);
        void *orig_dado = (char*)dados + ((i + 1) * arvore->tamanho_registro);
        memcpy(dest_dado, orig_dado, arvore->tamanho_registro);
    }
    cabecalho.n--;

    // 4. Salva a folha modificada no disco
    gravar_no(arvore, offset_atual, &cabecalho, chaves, dados, filhos);

    // 5. Aciona o gatilho de Underflow, se necessário
    int minimo_folha = (PFOLHA + 1) / 2;
    if (cabecalho.n < minimo_folha && offset_atual != arvore->offset_raiz) {
        // Chama a função recursiva de correção que injetamos anteriormente
        corrigir_underflow_disco(arvore, offset_atual, caminho_pais, topo);
    }

    // 6. Verificação pós-remoção do estado da Raiz da árvore
    // Carregamos a raiz atualizada do disco, pois os merges podem tê-la alterado
    CabecalhoNo cab_raiz;
    ler_no(arvore, arvore->offset_raiz, &cab_raiz, chaves, dados, filhos);

    if (cab_raiz.n == 0) {
        if (cab_raiz.folha) {
            // A árvore perdeu o seu último elemento
            arvore->offset_raiz = -1;
        } else {
            // A raiz interna sofreu merge e ficou vazia. O primeiro filho assume.
            long nova_raiz_offset = filhos[0];
            arvore->offset_raiz = nova_raiz_offset;
            
            // Atualiza o cabeçalho do novo nó raiz para indicar que ele não tem pai
            CabecalhoNo cab_nova_raiz;
            ler_no(arvore, nova_raiz_offset, &cab_nova_raiz, chaves, dados, filhos);
            cab_nova_raiz.offset_pai = -1;
            gravar_no(arvore, nova_raiz_offset, &cab_nova_raiz, chaves, dados, filhos);
        }
    }

    // Libera a memória de paginação temporária
    free(chaves); 
    free(dados); 
    free(filhos);
    
    return true;
}

// =======================================================
// Correção de Underflow em Disco (Redistribuição e Fusão)
// =======================================================
void corrigir_underflow_disco(ArvoreBPlus *arvore, long offset_no, long *caminho_pais, int topo) {
    // Se chegou na raiz, não há pai nem irmãos para emprestar.
    // O ajuste da raiz vazia já é tratado na função remover_registro.
    if (topo == 0) return; 

    long offset_pai = caminho_pais[topo - 1];

    // Alocações para o PAI e para o NÓ ALVO
    CabecalhoNo cab_pai, cab_no;
    void *chaves_pai = malloc(P * arvore->tamanho_chave);
    void *dados_pai = malloc(P * arvore->tamanho_registro);
    long *filhos_pai = malloc((P + 1) * sizeof(long));

    void *chaves_no = malloc(P * arvore->tamanho_chave);
    void *dados_no = malloc(P * arvore->tamanho_registro);
    long *filhos_no = malloc((P + 1) * sizeof(long));

    ler_no(arvore, offset_pai, &cab_pai, chaves_pai, dados_pai, filhos_pai);
    ler_no(arvore, offset_no, &cab_no, chaves_no, dados_no, filhos_no);

    // 1. Descobrir a posição do nó alvo dentro do array de filhos do pai
    int pos_filho = -1;
    for (int i = 0; i <= cab_pai.n; i++) {
        if (filhos_pai[i] == offset_no) {
            pos_filho = i; 
            break;
        }
    }

    // Identifica os offsets dos irmãos imediatos
    long offset_esq = (pos_filho > 0) ? filhos_pai[pos_filho - 1] : -1;
    long offset_dir = (pos_filho < cab_pai.n) ? filhos_pai[pos_filho + 1] : -1;

    // Calcula o limite mínimo de chaves
    int min_chaves = cab_no.folha ? (PFOLHA + 1) / 2 : (P + 1) / 2 - 1;

    // Variáveis para carregar os irmãos na RAM, se existirem
    CabecalhoNo cab_irmao;
    void *chaves_irmao = malloc(P * arvore->tamanho_chave);
    void *dados_irmao = malloc(P * arvore->tamanho_registro);
    long *filhos_irmao = malloc((P + 1) * sizeof(long));

    bool resolveu = false;

    // ====================================================================
    // CASO FOLHA
    // ====================================================================
    if (cab_no.folha) {
        
        // --- TENTATIVA 1: Emprestar do Irmão Esquerdo ---
        if (offset_esq != -1) {
            ler_no(arvore, offset_esq, &cab_irmao, chaves_irmao, dados_irmao, filhos_irmao);
            
            if (cab_irmao.n > min_chaves) {
                // 1. Desloca tudo no nó alvo para a direita para abrir espaço no índice 0
                for (int i = cab_no.n; i > 0; i--) {
                    memcpy((char*)chaves_no + (i * arvore->tamanho_chave), (char*)chaves_no + ((i - 1) * arvore->tamanho_chave), arvore->tamanho_chave);
                    memcpy((char*)dados_no + (i * arvore->tamanho_registro), (char*)dados_no + ((i - 1) * arvore->tamanho_registro), arvore->tamanho_registro);
                }

                // 2. Puxa a última chave/dado do irmão esquerdo para a primeira posição do nó alvo
                int ultimo_esq = cab_irmao.n - 1;
                memcpy(chaves_no, (char*)chaves_irmao + (ultimo_esq * arvore->tamanho_chave), arvore->tamanho_chave);
                memcpy(dados_no, (char*)dados_irmao + (ultimo_esq * arvore->tamanho_registro), arvore->tamanho_registro);
                
                cab_no.n++;
                cab_irmao.n--;

                // 3. Atualiza o separador no PAI (a nova primeira chave do nó alvo)
                memcpy((char*)chaves_pai + ((pos_filho - 1) * arvore->tamanho_chave), chaves_no, arvore->tamanho_chave);

                // 4. Salva no disco
                gravar_no(arvore, offset_no, &cab_no, chaves_no, dados_no, filhos_no);
                gravar_no(arvore, offset_esq, &cab_irmao, chaves_irmao, dados_irmao, filhos_irmao);
                gravar_no(arvore, offset_pai, &cab_pai, chaves_pai, dados_pai, filhos_pai);
                
                resolveu = true;
            }
        }

        // --- TENTATIVA 2: Emprestar do Irmão Direito ---
        if (!resolveu && offset_dir != -1) {
            ler_no(arvore, offset_dir, &cab_irmao, chaves_irmao, dados_irmao, filhos_irmao);
            
            if (cab_irmao.n > min_chaves) {
                // 1. Puxa a primeira chave/dado do irmão direito para o final do nó alvo
                memcpy((char*)chaves_no + (cab_no.n * arvore->tamanho_chave), chaves_irmao, arvore->tamanho_chave);
                memcpy((char*)dados_no + (cab_no.n * arvore->tamanho_registro), dados_irmao, arvore->tamanho_registro);
                
                cab_no.n++;

                // 2. Desloca tudo no irmão direito para a esquerda
                for (int i = 0; i < cab_irmao.n - 1; i++) {
                    memcpy((char*)chaves_irmao + (i * arvore->tamanho_chave), (char*)chaves_irmao + ((i + 1) * arvore->tamanho_chave), arvore->tamanho_chave);
                    memcpy((char*)dados_irmao + (i * arvore->tamanho_registro), (char*)dados_irmao + ((i + 1) * arvore->tamanho_registro), arvore->tamanho_registro);
                }
                cab_irmao.n--;

                // 3. Atualiza o separador no PAI (a nova primeira chave do irmão direito)
                memcpy((char*)chaves_pai + (pos_filho * arvore->tamanho_chave), chaves_irmao, arvore->tamanho_chave);

                // 4. Salva no disco
                gravar_no(arvore, offset_no, &cab_no, chaves_no, dados_no, filhos_no);
                gravar_no(arvore, offset_dir, &cab_irmao, chaves_irmao, dados_irmao, filhos_irmao);
                gravar_no(arvore, offset_pai, &cab_pai, chaves_pai, dados_pai, filhos_pai);

                resolveu = true;
            }
        }

        // --- TENTATIVA 3: Fundir (Merge) com o Irmão Esquerdo ---
        if (!resolveu && offset_esq != -1) {
            // Re-lê o irmão esquerdo (caso não tenha sido lido na tentativa 1)
            ler_no(arvore, offset_esq, &cab_irmao, chaves_irmao, dados_irmao, filhos_irmao);

            // 1. Despeja todo o conteúdo do nó alvo para o final do irmão esquerdo
            for (int i = 0; i < cab_no.n; i++) {
                memcpy((char*)chaves_irmao + ((cab_irmao.n + i) * arvore->tamanho_chave), (char*)chaves_no + (i * arvore->tamanho_chave), arvore->tamanho_chave);
                memcpy((char*)dados_irmao + ((cab_irmao.n + i) * arvore->tamanho_registro), (char*)dados_no + (i * arvore->tamanho_registro), arvore->tamanho_registro);
            }
            cab_irmao.n += cab_no.n;

            // 2. Mantém a integridade da Lista Encadeada
            cab_irmao.offset_prox_folha = cab_no.offset_prox_folha;

            // 3. Remove a chave separadora e o ponteiro do nó alvo lá no PAI
            for (int i = pos_filho - 1; i < cab_pai.n - 1; i++) {
                memcpy((char*)chaves_pai + (i * arvore->tamanho_chave), (char*)chaves_pai + ((i + 1) * arvore->tamanho_chave), arvore->tamanho_chave);
            }
            for (int i = pos_filho; i < cab_pai.n; i++) {
                filhos_pai[i] = filhos_pai[i + 1];
            }
            cab_pai.n--;

            // 4. Salva no disco
            gravar_no(arvore, offset_esq, &cab_irmao, chaves_irmao, dados_irmao, filhos_irmao);
            gravar_no(arvore, offset_pai, &cab_pai, chaves_pai, dados_pai, filhos_pai);
            // Obs: O bloco físico do `offset_no` ficará "órfão/lixo" no arquivo (fragmentação). 
            // Como é um trabalho acadêmico, não precisamos implementar um "Garbage Collector" de disco.

            resolveu = true;
        }

        // --- TENTATIVA 4: Fundir (Merge) com o Irmão Direito ---
        if (!resolveu && offset_dir != -1) {
            ler_no(arvore, offset_dir, &cab_irmao, chaves_irmao, dados_irmao, filhos_irmao);

            // 1. Despeja todo o conteúdo do irmão direito para o final do nó alvo
            for (int i = 0; i < cab_irmao.n; i++) {
                memcpy((char*)chaves_no + ((cab_no.n + i) * arvore->tamanho_chave), (char*)chaves_irmao + (i * arvore->tamanho_chave), arvore->tamanho_chave);
                memcpy((char*)dados_no + ((cab_no.n + i) * arvore->tamanho_registro), (char*)dados_irmao + (i * arvore->tamanho_registro), arvore->tamanho_registro);
            }
            cab_no.n += cab_irmao.n;
            cab_no.offset_prox_folha = cab_irmao.offset_prox_folha;

            // 2. Remove a chave separadora e o ponteiro do irmão direito no PAI
            for (int i = pos_filho; i < cab_pai.n - 1; i++) {
                memcpy((char*)chaves_pai + (i * arvore->tamanho_chave), (char*)chaves_pai + ((i + 1) * arvore->tamanho_chave), arvore->tamanho_chave);
            }
            for (int i = pos_filho + 1; i < cab_pai.n; i++) {
                filhos_pai[i] = filhos_pai[i + 1];
            }
            cab_pai.n--;

            // 3. Salva no disco
            gravar_no(arvore, offset_no, &cab_no, chaves_no, dados_no, filhos_no);
            gravar_no(arvore, offset_pai, &cab_pai, chaves_pai, dados_pai, filhos_pai);

            resolveu = true;
        }
    }

    // Libera a memória temporária das 4 laudas de RAM usadas
    free(chaves_pai); free(dados_pai); free(filhos_pai);
    free(chaves_no); free(dados_no); free(filhos_no);
    free(chaves_irmao); free(dados_irmao); free(filhos_irmao);

    // ====================================================================
    // PROPAGAÇÃO DO UNDERFLOW (A Mágica da Recursão)
    // ====================================================================
    // Se o nó pai, após perder uma chave em um Merge, ficou menor que o limite,
    // a árvore vai consertar o pai recursivamente!
    int min_interno = (P + 1) / 2 - 1;
    if (resolveu && cab_pai.n < min_interno) {
        corrigir_underflow_disco(arvore, offset_pai, caminho_pais, topo - 1);
    }
}