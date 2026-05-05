#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

void handler(void *arg);
int stream_send_file(int socket_fd, const char *filename);
