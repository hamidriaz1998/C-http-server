#define _GNU_SOURCE
#include "../include/network.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

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

bool setup_server(server_t *server) {
  if (server == NULL) {
    return false;
  }

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

  if (listen(server->socket, server->max_connections) == -1) {
    perror("listen");
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
  if (listen(server->socket, BACKLOG) == -1) {
    perror("listen");
    return false;
  }

  printf("Server Listening on port %d\n", server->port);

  struct sockaddr_storage client_addr; // Client address info
  char s[INET6_ADDRSTRLEN];
  while (server->is_running) {
    socklen_t size = sizeof(client_addr);
    int client_socket =
        accept(server->socket, (struct sockaddr *)&client_addr, &size);

    if (client_socket == -1) {
      perror("accept");
      continue;
    }
    if (!server->is_running) {
      close(client_socket);
      break;
    }

    inet_ntop(client_addr.ss_family,
              get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));
    printf("Got connection from %s\n", s);

    // Handle connections
    int *socket = (int *)malloc(
        sizeof(int)); // client socket on heap for passing to thread
    *socket = client_socket;
    thread_pool_add_task(server->thp, connection_handler, (void *)socket);
  }
  return true;
}

void connection_handler(void *arg) {
  int fd = *(int *)arg;
  free(arg);

  if (send(fd, "Welcome to the server\n", 23, 0) == -1) {
    perror("send");
  }
  close(fd);
}

void free_server(server_t *server) {
  if (server == NULL) {
    return;
  }
  thread_pool_free(server->thp);
  close(server->socket);
  free(server);
}
