#define _GNU_SOURCE
#include "include/handler.h"
#include "include/network.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static server_t *server_g = NULL;
void signal_handler(int signum) {
  printf("Received signal %d, shutting down server\n", signum);
  if (server_g != NULL) {
    server_g->is_running = false;

    if (server_g->socket != -1) {
      close(server_g->socket);
    }
  }
}

int main() {
  // Setup signal handlers
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);

  server_t *server = init_server(PORT, 100, 8);
  if (!setup_server(server)) {
    fprintf(stderr, "Failed to setup server\n");
    free_server(server);
    exit(EXIT_FAILURE);
  }
  server_g = server;

  printf("Server started on port %d .... Press Ctrl+C to stop\n", PORT);

  if (!run_server(server, handler)) {
    fprintf(stderr, "Failed to run server\n");
    free_server(server);
    exit(EXIT_FAILURE);
  }

  printf("Cleaning up...\n");
  free_server(server);
  server_g = NULL;

  return 0;
}
