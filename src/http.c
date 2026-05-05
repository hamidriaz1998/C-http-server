#define _GNU_SOURCE
#include "../include/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int string_to_method(char *method_str) {
  if (strcmp(method_str, "GET") == 0) {
    return HTTP_GET;
  } else if (strcmp(method_str, "POST") == 0) {
    return HTTP_POST;
  } else if (strcmp(method_str, "PUT") == 0) {
    return HTTP_PUT;
  } else if (strcmp(method_str, "DELETE") == 0) {
    return HTTP_DELETE;
  }
  return -1;
}

char *method_to_string(int method) {
  switch (method) {
  case HTTP_GET:
    return "GET";
  case HTTP_POST:
    return "POST";
  case HTTP_PUT:
    return "PUT";
  case HTTP_DELETE:
    return "DELETE";
  default:
    return NULL;
  }
}

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

  req->method = string_to_method(method);
  req->path = strdup(path);
  req->version = strdup(version);

  if (req->method == -1 || req->path == NULL || req->version == NULL) {
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

http_response *http_response_create(int status_code, char *body, int body_len,
                                    hashtable *headers) {
  http_response *res = malloc(sizeof(http_response));
  if (!res) {
    return NULL;
  }

  res->status_code = status_code;
  // res->headers = headers != NULL ? ht_copy(headers) : NULL;
  res->headers = headers != NULL ? headers : NULL;
  if (body_len == 0 || body == NULL) {
    res->body = NULL;
  } else {
    res->body = malloc(body_len);
    char content_length_str[32];
    snprintf(content_length_str, sizeof(content_length_str), "%d", body_len);
    ht_set(res->headers, "Content-Length", content_length_str);
    ht_set(res->headers, "Content-Type", "text/plain");
    if (res->body)
      memcpy(res->body, body, body_len);
  }

  return res;
}

void http_resp_to_str(http_response *res, char *response, size_t max_len) {
  int offset = 0;
  offset = snprintf(response, max_len, "HTTP/1.1 %d OK\r\n", res->status_code);
  if (res->headers != NULL) {
    for (size_t i = 0; i < res->headers->capacity; i++) {
      if (res->headers->entries[i].key != NULL) {
        offset += snprintf(response + offset, max_len - offset, "%s: %s\r\n",
                           res->headers->entries[i].key,
                           (char *)res->headers->entries[i].value);
        if ((size_t)offset >= max_len)
          return;
      }
    }
  }
  offset += snprintf(response + offset, max_len - offset, "\r\n");
}

void send_response(int fd, http_response *res) {
  size_t buffer_len = 4096;
  char response[buffer_len];
  http_resp_to_str(res, response, buffer_len);
  size_t total_sent = 0;
  size_t response_len = strlen(response);
  while (total_sent < response_len) {
    ssize_t sent =
        send(fd, response + total_sent, response_len - total_sent, 0);
    if (sent <= 0) {
      // Error or connection closed
      break;
    }
    total_sent += sent;
  }
}

void free_http_response(http_response *res) {
  if (!res) {
    return;
  }
  if (res->body)
    free(res->body);
  if (res->headers)
    ht_destroy(res->headers);
  free(res);
}
