# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Ficheiros do projeto (aqui colocamos funcionarios.o no plural)
OBJ = main.o B.o funcionarios.o
EXEC = rh_sistema

# Alvo principal (make)
all: $(EXEC)

# Regra de ligação
$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ)

# Regras de compilação
main.o: main.c Bplus1.h funcionarios.h
	$(CC) $(CFLAGS) -c main.c

B.o: B.c Bplus1.h
	$(CC) $(CFLAGS) -c B.c

funcionarios.o: funcionarios.c funcionarios.h
	$(CC) $(CFLAGS) -c funcionarios.c

# Alvo para executar o programa (make run)
run: all
	./$(EXEC)

# Limpeza geral exigida no trabalho (make clean)
clean:
	rm -f *.o $(EXEC)
	rm -f registos.bin indice_rh.bin