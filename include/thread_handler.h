#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define WORKER_BUFFER_SIZE 2048

void *acceptor_thread(void *arg);
void worker_thread(void *arg);
void connection_handler(void *arg);
