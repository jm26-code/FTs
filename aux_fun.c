#include "aux_fun.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

static int optval;

/*Funciones del SERVIDOR */
/*void server_recv(int sockfd, FileHeader);
void server_send(int sockfd, FileHeader);

 Fuciones del CLIENTE 
void download(int sockfd, FileHeader header);
void upload(int sockfd, FileHeader header);
*/
/* --FUNCIONES DE CONTROL --*/
/*void client_options(int socket, char** args, FileHeader header){
	//Operacion
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
			       download(socket, header);
			       break;
		case UPLOAD: printf("Subiendo el archivo  %s al servidor..\n", header.filename);
			     upload(socket, header);
			     break;
		default:
			     printf("Opcion no válida. Uso: %s -u o -d\n", args[0]);
	}
}

void server_options(int socket, FileHeader header){
	int flag = header.operation;

	switch (flag){
		case UPLOAD: printf("Servidor recibiendo el archivo...\n");
			     server_recv(socket, header);
			     break;
		case DOWNLOAD: printf("Servidor enviando el archivo...\n");
			       server_send(socket, header);
			       break;
		default:
			       printf("No se indica ninguna operacion.\n");
	}
}
*/
//-------------------------------------------------------------------------
//                                FUNCIONES
//-------------------------------------------------------------------------
void server_recv(int sockfd, FileHeader header){
	 //Recibir el archivo
         int buffer_recepcion = BUF_SIZE;
	 setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &buffer_recepcion, sizeof(buffer_recepcion));
 
	//Tomar solo el nombre del archivo
        char *File = basename(header.filename);

        int file_fd = open(File, O_WRONLY | O_CREAT | O_TRUNC, 0664);

        if(file_fd == -1){
                perror("open");
                close(sockfd);
                pthread_exit(NULL);;
        }

        //Bucle de recepción
        uint64_t all_recv = 0; 
	char *buffer = malloc(BUF_SIZE);

	if(buffer == NULL){
		perror("Error en malloc");
		close(file_fd);
		close(sockfd);
		pthread_exit(NULL);;
	}

	printf("Descargando: %s (%lu bytes)...\n", File, header.filesize);

        while(all_recv < header.filesize){
		//Calculamos como leer los datos
		uint64_t faltante = header.filesize - all_recv;
		size_t a_leer = (faltante < BUF_SIZE)? (size_t)faltante : BUF_SIZE; 
		
		ssize_t bytes_leidos = recv(sockfd, buffer, a_leer, MSG_WAITALL);

		if(bytes_leidos == -1){
			perror("Error durante la descarga");
			close(file_fd);
			close(sockfd);
			pthread_exit(NULL);
		}else if(bytes_leidos == 0){
			fprintf(stderr, "Conexion cerrada inesperadamente\n");
			break;
		}
	
		//Escribir los datos de forma segura
                if(write(file_fd, buffer, bytes_leidos) == -1){
			perror("Error al escribir en disco");
			close(file_fd);
			close(sockfd);
			pthread_exit(NULL);;
		}

                all_recv += bytes_leidos;

		//Registro
		printf("[Progreso]: %.2f%% (%lu / %lu bytes)\r", ((double)all_recv / header.filesize)*100, all_recv, header.filesize);
		fflush(stdout);
        }
	printf("\n"); //Salto de linea tras terminar.

	free(buffer);
        close(file_fd);
        close(sockfd);

        //Informe final:
	if(all_recv == header.filesize){
	      	printf("->Transmicion finalizada con éxito.\n->Archivo guardado como: %s\n", File);

	}else{
		fprintf(stderr, "Alerta: El archivo se descargó de manera incompleta.\n");

	}
}

void server_send(int sockfd, FileHeader header){

	//Verificar existencia del archivo
	int file_fd = open(header.filename, O_RDONLY);
	if(file_fd == -1){
		perror("open");
		close(sockfd);
		pthread_exit(NULL);;
	}

	//Obtener su tamaño
	struct stat file_stat;
	if(fstat(file_fd, &file_stat) == -1){
		perror("fstat");
		close(file_fd);
		close(sockfd);
		pthread_exit(NULL);
	}
	header.filesize = file_stat.st_size;

	int buffer_envio = BUF_SIZE;
	setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &buffer_envio, sizeof(buffer_envio));

	optval = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));

	//Confirmar enviando nombre y tamaño del archivo
	send(sockfd, &header, sizeof(FileHeader), 0);

	off_t offset = 0;
	size_t bytes_restantes = header.filesize;
	ssize_t bytes_enviados = 0;

	while(bytes_restantes > 0){
	
		bytes_enviados = sendfile(sockfd, file_fd, &offset, bytes_restantes);

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
			close(sockfd);
			pthread_exit(NULL);

		}else if(bytes_enviados == 0){
			//Cierre inesperado de archivo
			break;
		}
	
		bytes_restantes -= bytes_enviados;

		printf("[Bytes enviados en iteracion]: %ld\n", bytes_enviados);
	}

	optval = 0;
	setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));
		
	//Operacion finalizada.
	printf("->Archivo solicitado: %s\n->Enviados: %lu bytes\n", header.filename, header.filesize);

	close(file_fd);
	close(sockfd);

	pthread_exit(NULL);
}

//CLIENTE: -- FUNCION DE DESCARGA --
void download(int sockfd, FileHeader header){
        //Solicitud de archivo 	
        if(send(sockfd, &header, sizeof(FileHeader), 0) <= 0){
                perror("Error al enviar en File-name");
                close(sockfd);
                exit (EXIT_FAILURE);
        }

	int buffer_recepcion = BUF_SIZE;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &buffer_recepcion, sizeof(buffer_recepcion));

        //Recibir confirmacion e informacion del archivo;
        if(recv(sockfd, &header, sizeof(FileHeader), MSG_WAITALL) <= 0){
                perror("Error en la recepción de la cabecera del archivo");
                close(sockfd);
                exit (EXIT_FAILURE);
        }

	//Tomar solo el nombre del archivo
        char *File = basename(header.filename);

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

	printf("Descargando: %s (%lu bytes)...\n", File, header.filesize);

        while(all_recv < header.filesize){
		//Calculamos como leer los datos
		uint64_t faltante = header.filesize - all_recv;
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
		printf("[Progreso]: %.2f%% (%lu / %lu bytes)\r", ((double)all_recv / header.filesize)*100, all_recv, header.filesize);
		fflush(stdout);
        }
	printf("\n"); //Salto de linea tras terminar.

	free(buffer);
        close(file_fd);
        close(sockfd);

        //Informe final:
	if(all_recv == header.filesize){
	      	printf("->Transmicion finalizada con éxito.\n->Archivo guardado como: %s\n", File);
		exit (EXIT_SUCCESS);
	}else{
		fprintf(stderr, "Alerta: El archivo se descargó de manera incompleta.\n");
	}

}

//CLIENTE: -- FUNCION DE SUBIDA --
void upload(int sockfd, FileHeader header){
	//Enviar operacion, nombre y tamaño de archivo 	
	int file_fd = open(header.filename, O_RDONLY);
	if(file_fd == -1){
		perror("open");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	struct stat file_stat;
	if(fstat(file_fd, &file_stat) == -1){
		perror("fstat");
		close(file_fd);
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	header.filesize = file_stat.st_size;

	int buffer_envio = BUF_SIZE;
	setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &buffer_envio, sizeof(buffer_envio));

	optval = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));

	//Confirmar enviando nombre y tamaño del archivo
	send(sockfd, &header, sizeof(FileHeader), 0);

	off_t offset = 0;
	size_t bytes_restantes = header.filesize;
	ssize_t bytes_enviados = 0;

	while(bytes_restantes > 0){
	
		bytes_enviados = sendfile(sockfd, file_fd, &offset, bytes_restantes);

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
			close(sockfd);
			exit(EXIT_FAILURE);

		}else if(bytes_enviados == 0){
			//Cierre inesperado de archivo
			break;
		}
	
		bytes_restantes -= bytes_enviados;

		printf("[Bytes enviados en iteracion]: %ld\n", bytes_enviados);
	}

	optval = 0;
	setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));
		
	//Operacion finalizada.
	printf("->Archivo solicitado: %s\n->Enviados: %lu bytes\n", header.filename, header.filesize);

	close(file_fd);
	close(sockfd);
}

