# Makefile - Sistema de RH com Arvore B+ em disco
# Estrutura de Dados II - Avaliacao 2

CC = gcc
CFLAGS = -Wall -g
EXEC = rh_sistema

OBJS = main.o Bplus.o chave.o funcionario.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS)

main.o: main.c Bplus.h chave.h funcionario.h
	$(CC) $(CFLAGS) -c main.c

Bplus.o: Bplus.c Bplus.h
	$(CC) $(CFLAGS) -c Bplus.c

chave.o: chave.c chave.h Bplus.h
	$(CC) $(CFLAGS) -c chave.c

funcionario.o: funcionario.c funcionario.h chave.h
	$(CC) $(CFLAGS) -c funcionario.c

run: $(EXEC)
	./$(EXEC)

clean:
	rm -f $(OBJS) $(EXEC) indice.bin dados.bin

.PHONY: all run clean
