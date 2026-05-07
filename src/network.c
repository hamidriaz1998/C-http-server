#define _GNU_SOURCE
#include "../include/network.h"
#include "../include/thread_handler.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

server_t *init_server(int port, int max_connections, int thread_count) {
  // Allocate server
  server_t *server = (server_t *)malloc(sizeof(server_t));
  if (server == NULL) {
    return NULL;
  }
  server->port = port;
  server->max_connections = max_connections;

  // Allocate thread pool
  thread_pool_t *thp = thread_pool_init(thread_count);
  if (thp == NULL) {
    free(server);
    return NULL;
  }
  server->thp = thp;

  return server;
}

bool setup_server(server_t *server, void (*connection_handler)(void *)) {
  if (server == NULL) {
    return false;
  }

  server->connection_handler = connection_handler;

  struct addrinfo hints, *servinfo, *p;
  int yes = 1;
  int rv;
  char port_str[16];

  // Convert port to string
  snprintf(port_str, sizeof(port_str), "%d", server->port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return false;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((server->socket =
             socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(int)) == -1) {
      perror("setsockopt");
      freeaddrinfo(servinfo);
      close(server->socket);
      return false;
    }

    if (bind(server->socket, p->ai_addr, p->ai_addrlen) == -1) {
      close(server->socket);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    return false;
  }

  return true;
}

bool run_server(server_t *server) {
  if (server == NULL) {
    return false;
  }
  server->is_running = true;
  // Start listener
  if (listen(server->socket, server->max_connections) == -1) {
    perror("listen");
    return false;
  }
  printf("Server Listening on port %d\n", server->port);

  // Start Acceptor thread
  pthread_create(&server->acceptor_thread, NULL, acceptor_thread, server);

  return true;
}

void free_server(server_t *server) {
  if (server == NULL) {
    return;
  }
  thread_pool_free(server->thp);
  close(server->socket);
  free(server);
}
