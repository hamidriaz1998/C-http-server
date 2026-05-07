#define _GNU_SOURCE
#include "../include/thread_handler.h"
#include "../include/http.h"
#include "../include/network.h"
#include "../include/utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int stream_send_file(int socket_fd, int file_fd, struct stat *sb);
void handle_get(http_request *req, int socket_fd);
void *get_in_addr(struct sockaddr *sa);

void *acceptor_thread(void *arg) {
  server_t *server = (server_t *)arg;
  struct sockaddr_storage client_addr; // Client address info
  char s[INET6_ADDRSTRLEN];
  while (server->is_running) {
    socklen_t size = sizeof(client_addr);
    int client_socket =
        accept(server->socket, (struct sockaddr *)&client_addr, &size);

    if (client_socket == -1) {
      if (errno != // The shutdown() in main causes accept to return
          EINVAL)  // EINVAL and we don't want to print it on exiting the server

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
    thread_pool_add_task(server->thp, server->connection_handler,
                         (void *)socket);
  }
  return NULL;
}

void connection_handler(void *arg) {
  int socket_fd = *(int *)arg;
  char *buffer = (char *)calloc(1, WORKER_BUFFER_SIZE);
  free(arg);

  int bytes_received = recv(socket_fd, buffer, WORKER_BUFFER_SIZE - 1, 0);
  if (bytes_received > 0) {
    buffer[bytes_received] = '\0';
    printf("Received %d bytes of data\n", bytes_received);

    // Parse the HTTP request
    http_request *req = parse_request(buffer);
    if (req != NULL) {
#ifdef DEBUG
      printf("\n=== Parsed HTTP Request ===\n");
      printf("Method: %s\n", method_to_string(req->method));
      printf("Path: %s\n", req->path);
      printf("Version: %s\n", req->version);
      printf("\nHeaders:\n");
      if (req->headers != NULL) {
        for (size_t i = 0; i < req->headers->capacity; i++) {
          if (req->headers->entries[i].key != NULL) {
            printf("  %s: %s\n", req->headers->entries[i].key,
                   (char *)req->headers->entries[i].value);
          }
        }
      }
      if (req->body != NULL) {
        printf("\nBody: %s\n", req->body);
      }
      printf("============================\n\n");
#endif
      switch (req->method) {
      case HTTP_GET:
        handle_get(req, socket_fd);
        break;
      }
      free_http_request(req);
    } else {
      printf("Failed to parse HTTP request\n\n");
    }
    memset(buffer, 0, WORKER_BUFFER_SIZE);
  }

  free(buffer);
  close(socket_fd);
}

void handle_get(http_request *req, int socket_fd) {
  const char *doc_root = "./public";
  char full_path[PATH_MAX];

  if (resolve_request_path(req->path, doc_root, full_path, sizeof(full_path)) !=
      0) {
    printf("Error resolving request path: %s\n", req->path);
    http_response *res = http_response_create(HTTP_NOT_FOUND, NULL, 0, NULL);
    send_response(socket_fd, res);
    return;
  }

  int file_fd = open(full_path, O_RDONLY);
  if (file_fd == -1) {
    perror("open");
    return;
  }

  struct stat sb;
  if (fstat(file_fd, &sb) == -1) {
    perror("fstat");
    close(file_fd);
    return;
  }
  // Send status line and headers
  const char *content_type = get_mime_type(full_path);

  hashtable *headers = ht_create();
  char content_length[20];
  snprintf(content_length, sizeof(content_length), "%ld", sb.st_size);
  ht_set(headers, "Content-Type", (void *)content_type);
  ht_set(headers, "Content-Length", (void *)content_length);
  http_response *res = http_response_create(HTTP_OK, NULL, 0, headers);
  send_response(socket_fd, res);
  free_http_response(res);

  // Send file content
  if (stream_send_file(socket_fd, file_fd, &sb) == -1) {
    return;
  }
  close(file_fd);
}

int stream_send_file(int socket_fd, int file_fd, struct stat *sb) {
  off_t offset = 0;
  ssize_t bytes_sent = sendfile(socket_fd, file_fd, &offset, sb->st_size);
  if (bytes_sent == -1) {
    perror("sendfile");
    return -1;
  }
  return 0;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
