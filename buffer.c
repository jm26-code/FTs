#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define SERVER_PORT 8000

typedef struct {
    int id;
    int longitud_texto; // Es vital saber cuánto mide el texto
    char* str;
} Mensaje;

void enviar_estructura(int socket_fd, Mensaje* msg) {
    // 1. Calcular el tamaño total necesario: tamaño de los ints + los caracteres reales
    int tamano_texto = strlen(msg->str) + 1; // +1 para el '\0'
    msg->longitud_texto = tamano_texto;

    int tamano_total = sizeof(int) + sizeof(int) + tamano_texto;

    // 2. Reservar un buffer plano temporal
    char* buffer = (char*) malloc(tamano_total);

    // 3. Copiar los VALORES reales uno tras otro en el buffer
    char* posicion = buffer;

    memcpy(posicion, &(msg->id), sizeof(int));       // Copia el ID
    posicion += sizeof(int);

    memcpy(posicion, &(msg->longitud_texto), sizeof(int)); // Copia la longitud
    posicion += sizeof(int);

    memcpy(posicion, msg->str, tamano_texto);        // ¡Aquí copiamos el TEXTO real, no el puntero!

    // 4. Enviar todo el bloque plano por el socket
    send(socket_fd, buffer, tamano_total, 0);

    // 5. Liberar el buffer temporal
    free(buffer);
}

int main(int argc, char** argv){
        int sockfd;
        struct sockaddr_in com;

        memset(&com, 0, sizeof(com));
        com.sin_family = AF_INET;
        com.sin_port = htons(SERVER_PORT);
        com.sin_addr.s_addr = inet_addr(argv[1]);

        if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
                perror("socket");
                return EXIT_FAILURE;
        }

        if(connect(sockfd, (struct sockaddr*)&com, sizeof(com)) == -1){
                perror("connect");
                close(sockfd);
                return EXIT_FAILURE;
        }

        Mensaje *msg = (Mensaje*)malloc(sizeof(Mensaje));

        char arg[] = "Linea de comandos..";
        msg->str = (char*)malloc(sizeof(arg));
        strcpy(msg->str, arg);

        enviar_estructura(sockfd, msg);
        //printf("[Mensaje]: %s\n", msg->str);

	/*
        char send_msg[] = "[Client]: HELLO, I AM CLIENT!\n";
        send(sockfd, send_msg, strlen(send_msg)+1, 0);

        char recv_msg[256];
        recv(sockfd, recv_msg, sizeof(recv_msg), 0);

        puts(recv_msg);

	*/
        close(sockfd);

        return EXIT_SUCCESS;
}
