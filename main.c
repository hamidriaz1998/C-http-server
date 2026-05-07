#define _GNU_SOURCE
#include "include/network.h"
#include "include/thread_handler.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static server_t *server_g = NULL;

int main() {
  // Setup signal handlers
  sigset_t set;
  int sig;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGQUIT);

  if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
    perror("sigprocmask");
    exit(EXIT_FAILURE);
  }

  server_t *server = init_server(PORT, MAX_CONNECTIONS, DEFAULT_THREADS);
  if (!setup_server(server, connection_handler)) {
    fprintf(stderr, "Failed to setup server\n");
    free_server(server);
    exit(EXIT_FAILURE);
  }
  server_g = server;

  printf("Server started on port %d .... Press Ctrl+C to stop\n", PORT);

  if (!run_server(server)) {
    fprintf(stderr, "Failed to run server\n");
    free_server(server);
    exit(EXIT_FAILURE);
  }

  if (sigwait(&set, &sig) != 0) {
    fprintf(stderr, "sigwait failed\n");
    free_server(server);
    exit(EXIT_FAILURE);
  }

  printf("\nReceived signal %d. Shutting down...\n", sig);
  printf("Cleaning up...\n");

  server->is_running = false;
  shutdown(server->socket, SHUT_RDWR);
  pthread_join(server->acceptor_thread, NULL);

  free_server(server);
  server_g = NULL;

  return 0;
}
