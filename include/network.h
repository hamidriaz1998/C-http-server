#ifndef NETWORK_H
#define NETWORK_H

#include "thread_pool.h"
#include <sys/socket.h>

#define PORT 9000
#define MAX_CONNECTIONS 100
#define MAX_THREADS 10
#define BACKLOG 30
#define BUFFER_SIZE 1024

typedef struct server {
  thread_pool_t *thp;
  int socket;
  int port;
  int max_connections;
  int is_running;
} server_t;

server_t *init_server(int port, int max_connections, int thread_count);
bool run_server(server_t *server);
bool setup_server(server_t *server);
void free_server(server_t *server);
void connection_handler(void *arg);

#endif
