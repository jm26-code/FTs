#pragma once
#include <stdint.h>

#define SERVER_PORT "4000"
#define BUF_SIZE (8 * 1024 * 1024)
#define BACKLOG 10

struct __attribute__((packed)) FileHeader {
        char filename[256];
        uint64_t filesize;                           
};

struct __attribute__((packed)) FileRequest {
	char name[256];
};

void client(int arg_num, char** args);
void server(void);

