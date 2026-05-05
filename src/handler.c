#include "../include/handler.h"
#include "../include/http.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 2048

void handler(void *arg) {
  int fd = *(int *)arg;
  char *buffer = (char *)calloc(1, BUFFER_SIZE);
  free(arg);

  int bytes_received = recv(fd, buffer, BUFFER_SIZE, 0);
  if (bytes_received > 0) {
    buffer[bytes_received] = '\0';
    printf("Received %d bytes of data\n", bytes_received);
    printf("Raw data: \n%s\n", buffer);

    // Parse the HTTP request
    http_request *req = parse_request(buffer);
    if (req != NULL) {
#ifdef DEBUG
      printf("\n=== Parsed HTTP Request ===\n");
      printf("Method: %s\n", req->method);
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
      char *path = req->path;
      if (path[0] == '/') {
        path++;
      }
      if (strcmp(path, "") == 0) {
        path = "index.html";
      }
      char *full_path = (char *)calloc(1, strlen(path) + 1);
      strcpy(full_path, path);
      if (stream_send_file(fd, full_path) == -1) {
        perror("stream_send_file");
        free_http_request(req);
        return;
      }
      free_http_request(req);
    } else {
      printf("Failed to parse HTTP request\n\n");
    }
    memset(buffer, 0, BUFFER_SIZE);
  }

  free(buffer);
  close(fd);
}

int stream_send_file(int socket_fd, const char *filename) {
  int file_fd = open(filename, O_RDONLY);
  if (file_fd == -1) {
    perror("open");
    return -1;
  }

  struct stat sb;
  if (fstat(file_fd, &sb) == -1) {
    perror("fstat");
    close(file_fd);
    return -1;
  }

  off_t offset = 0;
  ssize_t bytes_sent = sendfile(socket_fd, file_fd, &offset, sb.st_size);
  if (bytes_sent == -1) {
    perror("sendfile");
    close(file_fd);
    return -1;
  }
  close(file_fd);
  return 0;
}
