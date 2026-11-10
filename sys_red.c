#include "sys_red.h"
#include "aux_fun.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

static int optval;
static int status;
static int server_fd;

/*------------------------------------------------------------------------*/
//                           C L I E N T E                                 
/*------------------------------------------------------------------------*/

//-- Creación y conexión del SOCKET --
void client(int arg_num, char** args){

	/* Condicion de inicio */
        if(arg_num == 2){
                fprintf(stderr, "Uso: %s -C <ip-address> <file-name>\n", args[0]);
                exit(EXIT_FAILURE);
        }

	/*-- Configurando la estructura --*/
        int sockfd;
        struct addrinfo hints, *res;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if((status = getaddrinfo(args[3], SERVER_PORT, &hints, &res)) != 0){
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
                exit(EXIT_FAILURE);
        }

        //Crear el SOCKET
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        if(sockfd == -1){
                perror("socket");
                freeaddrinfo(res);
                exit(EXIT_FAILURE);
        }

        if(connect(sockfd, res->ai_addr, res->ai_addrlen) == -1){
                perror("connect");
                freeaddrinfo(res);
                close(sockfd);
                exit(EXIT_FAILURE);
        }

        freeaddrinfo(res);

	//-- Envio y recepción de datos por el SOCKET --
	
	//Configuracion de Metadatos y recepcion de datos
	FileHeader header;
	memset(&header, 0, sizeof(FileHeader));

	//Nombre de archivo a solicitar
	strncpy(header.filename, args[4], sizeof(header.filename)-1);

	//Configurar ruta de archivo
	memset(header.destination_path, 0, sizeof(header.destination_path));
	if(arg_num == 6){
		strncpy(header.destination_path, args[5], sizeof(header.destination_path)-1);
	}

	/* --OPERACIONES DE CONTROL --*/
	if(strcmp(args[2], "-u") == 0){
		header.operation = UPLOAD;
	}
	else if(strcmp(args[2], "-d") == 0){
		header.operation = DOWNLOAD;
	}

	switch (header.operation){
		case DOWNLOAD: printf("Descargando el archivo %s...\n", header.filename);
			       download(sockfd, header);
			       break;
		case UPLOAD: printf("Subiendo el archivo  %s al servidor..\n", header.filename);
			     upload(sockfd, header);
			     break;
		default:
			     printf("Opcion no válida. Uso: %s -u o -d\n", args[0]);
	}

}

/*----------------------------------------------------------------------------*/
//                            S E R V I D O R
/*----------------------------------------------------------------------------*/

void server(void){

	/*-- SEÑALES --*/
	signal(SIGINT, manejador_sigint);
	signal(SIGPIPE, SIG_IGN);

	/*-- Configurando la estructura --*/
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((status = getaddrinfo(NULL, SERVER_PORT, &hints, &res)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	//Crear el SOCKET
	server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(server_fd == -1){
		perror("socket");
		exit(EXIT_FAILURE);
	}

	optval = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	//Enlazar
	if(bind(server_fd, res->ai_addr, res->ai_addrlen) == -1){
		perror("bind");
		freeaddrinfo(res);
		close(server_fd);
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(res);

	if(listen(server_fd, BACKLOG) == -1){
		perror("listen");
		close(server_fd);
		exit(EXIT_FAILURE);
	}

	puts("--------------------------------------------------------------------------");
		printf("Servidor escuchando en el puerto %s...\n", SERVER_PORT);
		puts("--------------------------------------------------------------------------");
		fflush(stdout);
	int cliente = 0;

	/*-- BUCLE DEL SERVIDOR CONCURRENTE --*/
	while(servidor_activo){

         	//Aceptar conecciones...	 
		int *client_fd = (int*)malloc(sizeof(int)); 
		if(client_fd == NULL){
			perror("malloc");
			continue;
		}

		*client_fd = accept(server_fd, NULL, NULL);
	
		//Manejamos errores en caso de interrupciones
		if(*client_fd == -1){
			if(!servidor_activo){
				free(client_fd);
				break;
			}
			
			perror("accept");
			free(client_fd);
			continue;
		}

		int sockfd = *client_fd;
		puts("\n-------------------------------------------------------------------------");
		printf("Servidor atentiendo al cliente %d...\n", ++cliente);
		puts("--------------------------------------------------------------------------");
		fflush(stdout);
	
		pthread_t t1;
		
		//Creamos el hilo
		if(pthread_create(&t1, NULL, attend_client, client_fd) != 0)
		{
			perror("pthread_create");
			free(client_fd);
			close(sockfd);
			continue;
		}

		//Salimos de forma limpia
		if(pthread_detach(t1) != 0)
		{
			perror("pthread_detach");
		}

		sleep(1);
		printf("Escuchando...");
		fflush(stdout);
	}
	
	close(server_fd);
}

/*-- FUNCION DE INTERACIÓN CON EL CLIENTE --*/
void* attend_client(void *arg){

	int client_fd = *(int*)arg;
	free(arg);
	arg = NULL;

	//Recibir la estructura de peticion completa
	FileHeader header;
	memset(&header, 0, sizeof(FileHeader));

	if(recv(client_fd, &header, sizeof(FileHeader), MSG_WAITALL) <= 0){
		perror("Error al recibir el nombre del archivo");
		close(client_fd);
		pthread_exit(NULL);
	}

	//-- OPERACIONES DE CONTROL -- 
	int flag = header.operation;

	switch (flag){
		case UPLOAD: printf("Servidor recibiendo el archivo...\n");
			     server_recv(client_fd, header);
			     break;
		case DOWNLOAD: printf("Servidor enviando el archivo...\n");
			       server_send(client_fd, header);
			       break;
		default:
			       printf("No se ha indicado ninguna operacion.\n");
	}

		return NULL;
}

//-------------------------------------------------------------------------
/*-- MANEJADOR DE SEÑALES --*/
void manejador_sigint(int sig){
	printf("\n[Servidor]: Señal de apagado recibida. Cerrando conexiones de forma limpia...\n");
	servidor_activo = 0;

	close(server_fd);

	pthread_detach(pthread_self());

}
