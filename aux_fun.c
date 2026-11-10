#include "aux_fun.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>


//------------------------------------------------------------------------
//                              SERVIDOR:
//------------------------------------------------------------------------
/*-- FUNCION DE RECEPCION DE ARCHIVOS --*/
void server_recv(int sockfd, FileHeader header){
	 //Recibir el archivo
         int buffer_recepcion = BUF_SIZE;
	 setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &buffer_recepcion, sizeof(buffer_recepcion));
 
	//Configurar archivo
	char *path_ptr = full_path(header);
	if(path_ptr == NULL) return;

	int len = strlen(path_ptr);
	char path_name[len];
	strcpy(path_name, path_ptr);
	free(path_ptr); path_ptr = NULL;

    int file_fd = open(path_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);

        if(file_fd == -1){
                perror("open");
				puts(path_name);
                close(sockfd);
                return;
        }

        //Bucle de recepción
        uint64_t all_recv = 0; 
	char *buffer = malloc(BUF_SIZE);

	if(buffer == NULL){
		perror("Error en malloc");
		close(file_fd);
		close(sockfd);
		return;
	}

	printf("Descargando: %s (%lu bytes)...\n", path_name, header.filesize);

    while(all_recv < header.filesize){
		//Calculamos como leer los datos
		uint64_t faltante = header.filesize - all_recv;
		size_t a_leer = (faltante < BUF_SIZE)? (size_t)faltante : BUF_SIZE; 
		
		ssize_t bytes_leidos = recv(sockfd, buffer, a_leer, MSG_WAITALL);

		if(bytes_leidos == -1){
			perror("Error durante la descarga");
			close(file_fd);
			close(sockfd);
			return;
		}else if(bytes_leidos == 0){
			fprintf(stderr, "Conexion cerrada inesperadamente\n");
			break;
		}
	
		//Escribir los datos de forma segura
        if(write(file_fd, buffer, bytes_leidos) == -1){
			perror("Error al escribir en disco");
			close(file_fd);
			close(sockfd);
			return;
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
	      	printf("->Transmicion finalizada con éxito.\n->Archivo guardado como: %s\n", path_name);

	}else{
		fprintf(stderr, "Alerta: El archivo se descargó de manera incompleta.\n");

	}
}


 /*-- FUNCION DE ENVÍO DE ARCHIVOS --*/ 
void server_send(int sockfd, FileHeader header){

	//Verificar existencia del archivo
	int file_fd = open(header.filename, O_RDONLY);
	if(file_fd == -1){
		perror("open");
		close(sockfd);
		return;
	}

	//Obtener su tamaño
	struct stat file_stat;
	if(fstat(file_fd, &file_stat) == -1){
		perror("fstat");
		close(file_fd);
		close(sockfd);
		return;
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
			return;

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

//-------------------------------------------------------------------------
//                              CLIENTE:
//-------------------------------------------------------------------------
/*-- FUNCION DE DESCARGA DE ARCHIVOS --*/
void download(int sockfd, FileHeader header){
        //Solicitud de archivo 	
        if(send(sockfd, &header, sizeof(FileHeader), 0) <= 0){
                perror("Error al enviar en File-name");
                close(sockfd);
                return;
        }

	int buffer_recepcion = BUF_SIZE;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &buffer_recepcion, sizeof(buffer_recepcion));

        //Recibir confirmacion e informacion del archivo;
        if(recv(sockfd, &header, sizeof(FileHeader), MSG_WAITALL) <= 0){
                perror("Error en la recepción de la cabecera del archivo");
                close(sockfd);
                return;
        }

	//Configuracion del archivo
	char *path_ptr = full_path(header);
	if(path_ptr == NULL) return;

	int len = strlen(path_ptr);
	char path_name[len];
	strcpy(path_name, path_ptr);
	free(path_ptr); path_ptr = NULL;

        int file_fd = open(path_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);

        if(file_fd == -1){
                perror("open");
                close(sockfd);
                return;
        }

        //Bucle de recepción
        uint64_t all_recv = 0; 
	char *buffer = malloc(BUF_SIZE);//buffer[BUF_SIZE];

	if(buffer == NULL){
		perror("Error en malloc");
		close(file_fd);
		close(sockfd);
		return;
	}

	printf("Descargando: %s (%lu bytes)...\n", path_name, header.filesize);

    while(all_recv < header.filesize){
		//Calculamos como leer los datos
		uint64_t faltante = header.filesize - all_recv;
		size_t a_leer = (faltante < BUF_SIZE)? (size_t)faltante : BUF_SIZE; 
		
		ssize_t bytes_leidos = recv(sockfd, buffer, a_leer, MSG_WAITALL);

		if(bytes_leidos == -1){
			perror("Error durante la descarga");
			close(file_fd);
			close(sockfd);
			return;
		}else if(bytes_leidos == 0){
			fprintf(stderr, "Conexion cerrada inesperadamente\n");
			break;
		}
	
		//Escribir los datos de forma segura
        if(write(file_fd, buffer, bytes_leidos) == -1){
			perror("Error al escribir en disco");
			close(file_fd);
			close(sockfd);
			return;
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
	      	printf("->Transmicion finalizada con éxito.\n->Archivo guardado como: %s\n", path_name);
	
			exit(EXIT_SUCCESS);
	}else{
		fprintf(stderr, "Alerta: El archivo se descargó de manera incompleta.\n");
	}

}

/*-- FUNCION DE SUBIDA DE ARCHIVOS --*/
void upload(int sockfd, FileHeader header){
	//Enviar operacion, nombre y tamaño de archivo 	
	int file_fd = open(header.filename, O_RDONLY);
	if(file_fd == -1){
		perror("open");
		close(sockfd);
		return;
	}

	struct stat file_stat;
	if(fstat(file_fd, &file_stat) == -1){
		perror("fstat");
		close(file_fd);
		close(sockfd);
		return;
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
			return;

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

//Configurar ruta de archivo
char* full_path(FileHeader header){
	//Nombre de archivo
    char *File = basename(header.filename);
	int len = strlen(File) + 1;

    char *path_name = (char*)calloc(1, len);
	if(path_name == NULL) {
		perror("calloc");
		return NULL;
	}

	strncpy(path_name, File, len);

	//Ruta en caso de fijarse, de lo contrario, se aloja en el directorio local.
	if(header.destination_path[0] != '\0'){
		char *dir = header.destination_path;
		len = snprintf(NULL, 0, "%s/%s", dir, File) + 1;

		char* ptr = (char*)realloc(path_name, len);
		path_name = ptr;

		snprintf(path_name, len, "%s/%s", dir, File);
	}

	return path_name;
}
