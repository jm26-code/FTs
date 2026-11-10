#Variables para facilitar cambios
CC = gcc
CFLAGS = -Wall -g
LIB = -lpthread
OBJ = main.o sys_red.o aux_fun.o

programa: main

sys_red.o: sys_red.h sys_red.c
	$(CC) -c sys_red.c $(CFLAGS)

aux_fun.o: aux_fun.h aux_fun.c
	$(CC) -c aux_fun.c $(CFLAGS)

main.o: main.c sys_red.h aux_fun.h
	$(CC) -c main.c $(CFLAGS)

main: $(OBJ)
	$(CC) $(OBJ) -o main $(LIB) 

#Objetivo PHONY (acción) para limpiar archivos temporales
clean:
	rm main
	rm -f *.o programa
