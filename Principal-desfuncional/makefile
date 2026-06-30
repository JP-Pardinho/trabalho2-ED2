CC = gcc
CFLAGS = -Wall -g
OBJ = Bplus.o main.o
EXEC = sistema_rh

make: $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ)

Bplus.o: Bplus.c Bplus.h
	$(CC) $(CFLAGS) -c Bplus.c

main.o: main.c Bplus.h
	$(CC) $(CFLAGS) -c main.c

run: make
	./$(EXEC)

clean:
	rm -f *.o $(EXEC) indice_rh.bin dados_rh.bin