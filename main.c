#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys_red.h"

int main(int argc, char** argv){
	puts("-- FILE TRANSFER SERVER --");

        if(argc == 1){
                fprintf(stderr, "Use: %s, luego <-C> o <-S>\n", argv[0]);
                exit(EXIT_FAILURE);
        }

	if(argc == 1){
		fprintf(stderr, "Faltan argumentos\n");
		return EXIT_FAILURE;
	}

	if(strcmp(argv[1], "-S") == 0){
		server();
	}

	else if(strcmp(argv[2], "-C") == 0){
		//Programa del cliente...
		client(argc, argv);
	}
	else{
		printf("Parametro incorrecto. Use %s, luego '-S' o  '-C'.\n", argv[0]);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
