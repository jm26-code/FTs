#pragma once
#include <stdint.h>
#include <signal.h>

#define SERVER_PORT "4000"
#define BACKLOG 10
/*
struct __attribute__((packed)) FileHeader {
        char filename[256];
        uint64_t filesize;                           
};

struct __attribute__((packed)) FileRequest {
	char name[256];
};
*/
static volatile sig_atomic_t servidor_activo = 1;

void manejador_sigint(int sig);
void* attend_client(void* arg);

void client(int arg_num, char** args);
void server(void);

