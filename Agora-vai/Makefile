# ============================================================================
# Makefile - Sistema de RH com Arvore B+ em disco (ED2 - Avaliacao 2)
# Ambiente: Linux
# ============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11
TARGET = rh_sistema

SRCS = main.c bplus.c funcionario.c
OBJS = $(SRCS:.c=.o)

# Arquivos binarios gerados em tempo de execucao (indice + dados)
DATA_FILES = rh_indice.dat rh_funcionarios.dat

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(DATA_FILES)
