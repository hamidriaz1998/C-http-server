#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

void handler(void *arg);

int stream_send_file(int socket_fd, int file_fd, struct stat *sb);
void handle_get(http_request *req, int socket_fd);
