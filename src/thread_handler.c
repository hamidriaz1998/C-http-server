#define _GNU_SOURCE
#include "../include/thread_handler.h"
#include "../include/http.h"
#include "../include/utils.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static const char *doc_root = "./public";

void set_doc_root(const char *root) {
    doc_root = root;
}

int stream_send_file(int socket_fd, int file_fd, struct stat *sb);
void handle_get(http_request *req, int socket_fd);

void connection_handler(void *arg) {
    int socket_fd = *(int *)arg;
    char buffer[WORKER_BUFFER_SIZE];
    free(arg);

    struct timeval tv = {5, 0};
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int bytes_received = recv(socket_fd, buffer, WORKER_BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';

        http_request *req = parse_request(buffer);
        if (req) {
            switch (req->method) {
            case HTTP_GET:
                handle_get(req, socket_fd);
                break;
            }
            free_http_request(req);
        }
    }

    close(socket_fd);
}

void handle_get(http_request *req, int socket_fd) {
    char full_path[PATH_MAX];

    if (resolve_request_path(req->path, doc_root, full_path, sizeof(full_path)) != 0) {
        http_response *res = http_response_create(HTTP_NOT_FOUND, NULL, 0, NULL);
        send_response(socket_fd, res);
        free_http_response(res);
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

    const char *content_type = get_mime_type(full_path);
    hashtable *headers = ht_create();
    char content_length[20];
    snprintf(content_length, sizeof(content_length), "%ld", sb.st_size);
    ht_set(headers, "Content-Type", (void *)content_type);
    ht_set(headers, "Content-Length", (void *)content_length);
    http_response *res = http_response_create(HTTP_OK, NULL, 0, headers);
    send_response(socket_fd, res);
    free_http_response(res);

    if (stream_send_file(socket_fd, file_fd, &sb) == -1) {
        close(file_fd);
        return;
    }
    close(file_fd);
}

int stream_send_file(int socket_fd, int file_fd, struct stat *sb) {
    off_t offset = 0;
    ssize_t bytes_sent = sendfile(socket_fd, file_fd, &offset, sb->st_size);
    if (bytes_sent == -1) {
        if (errno == EPIPE || errno == ECONNRESET)
            return 0;
        perror("sendfile");
        return -1;
    }
    return 0;
}
