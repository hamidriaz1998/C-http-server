#ifndef _HTTP_H
#define _HTTP_H

#include "hashtable.h"
#include <sys/socket.h>

enum http_status {
  HTTP_OK = 200,
  HTTP_BAD_REQUEST = 400,
  HTTP_NOT_FOUND = 404,
  HTTP_INTERNAL_SERVER_ERROR = 500,
};

#define HTTP_METHOD_INVALID -1
#define HTTP_HEADER_SEPARATOR_LEN 4
#define RESPONSE_BUFFER_SIZE 4096
#define CONTENT_LENGTH_BUF_SIZE 32

enum http_method {
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
};

typedef struct {
  int method;
  char *path;
  char *version;
  hashtable *headers;
  char *body;
} http_request;

typedef struct {
  int status_code;
  char *body;
  hashtable *headers;
} http_response;

int string_to_method(char *method_str);
char *method_to_string(int method);

http_request *parse_request(char *req_str);
void free_http_request(http_request *req);

http_response *http_response_create(int status_code, char *body, int body_len,
                                    hashtable *headers);
void send_response(int fd, http_response *res);
void free_http_response(http_response *res);
#endif
