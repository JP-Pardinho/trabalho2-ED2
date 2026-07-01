# Nome do executável
TARGET = sistema_rh

# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Arquivos fonte e objetos
SRCS = main.c Bplus.c funcionario.c
OBJS = $(SRCS:.c=.o)

# Comando padrão: compilar
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

# Regra para compilar os arquivos .c em .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Executar o programa
run: all
	./$(TARGET)

# Limpeza dos arquivos gerados
clean:
	rm -f $(OBJS) $(TARGET) *.bin

.PHONY: all run clean