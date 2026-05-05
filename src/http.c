#define _GNU_SOURCE
#include "../include/http.h"
#include <stdlib.h>
#include <string.h>

http_request *parse_request(char *req_str) {
  http_request *req = calloc(1, sizeof(http_request));
  if (req == NULL) {
    return NULL;
  }

  char *req_copy = strdup(req_str);
  if (req_copy == NULL) {
    free(req);
    return NULL;
  }

  // Find the separator between headers and body (\r\n\r\n)
  char *body_start = strstr(req_copy, "\r\n\r\n");
  char *headers_section = req_copy;

  if (body_start != NULL) {
    *body_start = '\0'; // Terminate headers section
    body_start += 4;    // Move past \r\n\r\n to start of body
  }

  // Parse request line
  char *saveptr1 = NULL;
  char *saveptr2 = NULL;
  char *line = strtok_r(headers_section, "\r\n", &saveptr1);
  if (line == NULL) {
    free(req_copy);
    free(req);
    return NULL;
  }

  char *line_copy = strdup(line);
  char *method = strtok_r(line_copy, " ", &saveptr2);
  char *path = strtok_r(NULL, " ", &saveptr2);
  char *version = strtok_r(NULL, " ", &saveptr2);

  if (method == NULL || path == NULL || version == NULL) {
    free(line_copy);
    free(req_copy);
    free(req);
    return NULL;
  }

  req->method = strdup(method);
  req->path = strdup(path);
  req->version = strdup(version);

  if (req->method == NULL || req->path == NULL || req->version == NULL) {
    free_http_request(req);
    free(line_copy);
    free(req_copy);
    free(req);
    return NULL;
  }

  // Parse headers
  hashtable *headers = ht_create();
  char *header_line = strtok_r(NULL, "\r\n", &saveptr1);
  while (header_line != NULL) {
    char *colon = strchr(header_line, ':');
    if (colon != NULL) {
      *colon = '\0'; // Split the string
      char *key = header_line;
      char *value = colon + 1;
      // Skip leading whitespace in value
      while (*value == ' ' || *value == '\t')
        value++;
      ht_set(headers, key, value);
    }
    header_line = strtok_r(NULL, "\r\n", &saveptr1);
  }
  req->headers = headers;

  // Parse body if any
  if (headers != NULL && ht_get(headers, "Content-Length") != NULL &&
      body_start != NULL) {
    req->body = strdup(body_start);
  } else {
    req->body = NULL;
  }

  free(line_copy);
  free(req_copy);
  return req;
}

void free_http_request(http_request *req) {
  if (!req) {
    return;
  }
  if (req->method)
    free(req->method);
  if (req->path)
    free(req->path);
  if (req->version)
    free(req->version);
  if (req->headers)
    ht_destroy(req->headers);
  if (req->body)
    free(req->body);
  free(req);
}
