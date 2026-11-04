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

int optval;
int status;;
volatile sig_atomic_t servidor_activo = 1;

void manejador_sigint(int sig);
void* attend_client(void* arg);


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
	
	//Configuracion de Mtadatos y recepcion de datos
	FileHeader header;
	memset(&header, 0, sizeof(FileHeader));
	//client_options(sockfd, args, header);

	/* --OPERACIONES DE CONTROL --*/
	if(strcmp(args[1], "-u") == 0){
		header.operation = UPLOAD;
	}
	else if(strcmp(args[1], "-d") == 0){
		header.operation = DOWNLOAD;
	}

	//Nombre de archivo a solicitar
	strncpy(header.filename, args[4], sizeof(header.filename)-1);

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

	//Solicitud de archivo
	/*struct FileRequest file;
	memset(&file, 0, sizeof(file));
	strncpy(file.name, args[2], sizeof(file.name)-1);

        if(send(sockfd, &file, sizeof(struct FileRequest), 0) <= 0){
                perror("Error al enviar en File-name");
                close(sockfd);
                exit (EXIT_FAILURE);
        }

	int buffer_recepcion = BUF_SIZE;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &buffer_recepcion, sizeof(buffer_recepcion));

        //Recibir confirmacion e informacion del archivo en cuestion	  
      	struct FileHeader f_info;
        memset(&f_info, 0, sizeof(f_info));
        if(recv(sockfd, &f_info, sizeof(struct FileHeader), MSG_WAITALL) <= 0){
                perror("Error en la recepción de la cabecera del archivo");
                close(sockfd);
                exit (EXIT_FAILURE);
        }

	//Tomar solo el nombre del archivo
        char *File = basename(f_info.filename);

        int file_fd = open(File, O_WRONLY | O_CREAT | O_TRUNC, 0664);

        if(file_fd == -1){
                perror("open");
                close(sockfd);
                exit (EXIT_FAILURE);
        }

        //Bucle de recepción
        uint64_t all_recv = 0; 
	char *buffer = malloc(BUF_SIZE);//buffer[BUF_SIZE];

	if(buffer == NULL){
		perror("Error en malloc");
		close(file_fd);
		close(sockfd);
		exit (EXIT_FAILURE);
	}

	printf("Descargando: %s (%lu bytes)...\n", File, f_info.filesize);

        while(all_recv < f_info.filesize){
		//Calculamos como leer los datos
		uint64_t faltante = f_info.filesize - all_recv;
		size_t a_leer = (faltante < BUF_SIZE)? (size_t)faltante : BUF_SIZE; 
		
		ssize_t bytes_leidos = recv(sockfd, buffer, a_leer, MSG_WAITALL);

		if(bytes_leidos == -1){
			perror("Error durante la descarga");
			close(file_fd);
			close(sockfd);
			exit (EXIT_FAILURE);
		}else if(bytes_leidos == 0){
			fprintf(stderr, "Conexion cerrada inesperadamente\n");
			break;
		}
	
		//Escribir los datos de forma segura
                if(write(file_fd, buffer, bytes_leidos) == -1){
			perror("Error al escribir en disco");
			close(file_fd);
			close(sockfd);
			exit (EXIT_FAILURE);
		}

                all_recv += bytes_leidos;

		//Registro
		printf("[Progreso]: %.2f%% (%lu / %lu bytes)\r", ((double)all_recv / f_info.filesize)*100, all_recv, f_info.filesize);
		fflush(stdout);
        }
	printf("\n"); //Salto de linea tras terminar.

	free(buffer);
        close(file_fd);
        close(sockfd);

        //Informe final:
	if(all_recv == f_info.filesize){      	printf("->Transmicion finalizada con éxito.\n->Archivo guardado como: %s\n", File);
		exit (EXIT_SUCCESS);
	}else{
		fprintf(stderr, "Alerta: El archivo se descargó de manera incompleta.\n");
		exit (EXIT_FAILURE);
	}*/
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
	int server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
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

	printf("Servidor escuchando en el puerto %s...\n", SERVER_PORT);

	/*-- BUCLE DEL SERVIDOR CONCURRENTE --*/
	while(servidor_activo){
         	//Aceptar conecciones...	 
		int *client_fd = (int*)malloc(sizeof(int)); 
		if(client_fd == NULL){
			perror("malloc");
			continue;
		}

		*client_fd = accept(server_fd, NULL, NULL);
	
		//Manejamos los errores en caso de interrupciones
		if(*client_fd == -1){
			if(!servidor_activo){
				free(client_fd);
				close(server_fd);
				//break;
				exit(EXIT_FAILURE);
			}
			
			perror("accept");
			free(client_fd);
			continue;
		}

		int sockfd = *client_fd;

		printf("Conexion establecida!\n");
		
		pthread_t t1;
		
		//Creamos el hilo
		if(pthread_create(&t1, NULL, attend_client, client_fd) != 0)
		{
			perror("pthread_create");
			free(client_fd);
			close(sockfd);
		}

		//Slimos de forma limpia
		if(pthread_detach(t1) != 0)
		{
			perror("pthread_detach");
		}
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
	//server_options(client_fd, header);

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

	/*struct FileRequest request;
	memset(&request, 0, sizeof(request));

	if(recv(client_fd, &request, sizeof(struct FileRequest), MSG_WAITALL) <= 0){
		perror("Error al recibir el nombre del archivo");
		close(client_fd);
		exit(EXIT_FAILURE);
	}
		
	struct FileHeader f;
	memset(&f, 0, sizeof(f));
	strncpy(f.filename, request.name, sizeof(f.filename)-1);

	int file_fd = open(f.filename, O_RDONLY);
	if(file_fd == -1){
		perror("open");
		close(client_fd);
		exit(EXIT_FAILURE);
	}

	struct stat file_stat;
	if(fstat(file_fd, &file_stat) == -1){
		perror("fstat");
		close(file_fd);
		close(client_fd);
		exit(EXIT_FAILURE);
	}
	f.filesize = file_stat.st_size;

	int buffer_envio = BUF_SIZE;
	setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &buffer_envio, sizeof(buffer_envio));

	optval = 1;
	setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));

	//Confirmar enviando nombre y tamaño del archivo
	send(client_fd, &f, sizeof(struct FileHeader), 0);

	off_t offset = 0;
	size_t bytes_restantes = f.filesize;
	ssize_t bytes_enviados = 0;

	while(bytes_restantes > 0){
	
		bytes_enviados = sendfile(client_fd, file_fd, &offset, bytes_restantes);

		if(bytes_enviados == -1){

			if(errno == EINTR || errno == EAGAIN){
				continue;
			}

			if(errno == EPIPE || errno == ECONNRESET){
				printf("[INFO]: El cliente se desconectó durante la transferencia\n");	
			}else{
				printf("Error crítico en senfile\n");
			}

			close(file_fd);
			close(client_fd);
			pthread_exit(NULL);

		}else if(bytes_enviados == 0){
			//Cierre inesperado de archivo
			break;
		}
	
		bytes_restantes -= bytes_enviados;

		printf("[Bytes enviados en iteracion]: %ld\n", bytes_enviados);
	}

	optval = 0;
	setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));
		
	//Operacion finalizada.
	printf("->Archivo solicitado: %s\n->Enviados: %lu bytes\n", f.filename, f.filesize);

	close(file_fd);
	close(client_fd);
*/
	pthread_exit(NULL);
}

//-------------------------------------------------------------------------
/*-- MANEJADOR DE SEÑALES --*/
void manejador_sigint(int sig){
	printf("\n[Servidor] Señal de apagado recibida. Cerrando conexiones de forma limpia...\n");
	servidor_activo = 0;
}
