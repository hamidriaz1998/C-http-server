#ifndef _HTTP_H
#define _HTTP_H

#include "hashtable.h"

typedef struct {
  char *method;
  char *path;
  char *version;
  hashtable *headers;
  char *body;
} http_request;

http_request *parse_request(char *req_str);
void free_http_request(http_request *req);
#endif
