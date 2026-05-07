#ifndef NETWORK_H
#define NETWORK_H

#include "thread_pool.h"
#include <signal.h>
#include <sys/socket.h>

#define PORT 9000
#define MAX_CONNECTIONS 10000
#define MAX_THREADS 10
#define DEFAULT_THREADS 8
#define BUFFER_SIZE 4096

typedef struct server {
  thread_pool_t *thp;
  int socket;
  int port;
  int max_connections;
  pthread_t acceptor_thread;
  volatile sig_atomic_t is_running;
  void (*connection_handler)(void *);
} server_t;

server_t *init_server(int port, int max_connections, int thread_count);
bool run_server(server_t *server);
bool setup_server(server_t *server, void (*connection_handler)(void *));
void free_server(server_t *server);
void connection_handler(void *arg);

#endif
