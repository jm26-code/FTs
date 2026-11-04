#pragma once
#include <stdint.h>

#ifndef BUF_SIZE
#define BUF_SIZE 4 * 1024 * 1024
#endif

typedef enum {
	UPLOAD,
	DOWNLOAD
} FileOption;

typedef struct __attribute__((packed)) {
	FileOption operation;
	char filename[256];
	char destination_path[256];
	uint64_t filesize;
} FileHeader;

/*Funciones del SERVIDOR */
void server_recv(int sockfd, FileHeader header);
void server_send(int sockfd, FileHeader header);

/* Fuciones del CLIENTE */
void download(int sockfd, FileHeader header);
void upload(int sockfd, FileHeader header);

