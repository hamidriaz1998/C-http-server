#include "../include/hashtable.h"
#include "../include/http.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_request(http_request *req) {
  if (req == NULL) {
    printf("Request is NULL\n");
    return;
  }

  printf("=== HTTP Request ===\n");
  printf("Method: %s\n", req->method ? req->method : "NULL");
  printf("Path: %s\n", req->path ? req->path : "NULL");
  printf("Version: %s\n", req->version ? req->version : "NULL");

  printf("Headers:\n");
  if (req->headers != NULL) {
    // Note: This assumes hashtable has a way to iterate. For now, we'll just
    // check specific headers
    char *content_length = (char *)ht_get(req->headers, "Content-Length");
    char *content_type = (char *)ht_get(req->headers, "Content-Type");
    char *host = (char *)ht_get(req->headers, "Host");

    if (content_length)
      printf("  Content-Length: %s\n", content_length);
    if (content_type)
      printf("  Content-Type: %s\n", content_type);
    if (host)
      printf("  Host: %s\n", host);
  } else {
    printf("  No headers\n");
  }

  printf("Body: %s\n", req->body ? req->body : "NULL");
  if (req->body) {
    printf("Body length: %zu\n", strlen(req->body));
  }
  printf("==================\n\n");
}

void test_simple_get_request() {
  printf("Testing simple GET request...\n");

  char *request = "GET /index.html HTTP/1.1\r\n"
                  "Host: localhost:8080\r\n"
                  "User-Agent: TestClient/1.0\r\n"
                  "\r\n";

  http_request *req = parse_request(request);
  print_request(req);

  assert(req != NULL);
  assert(strcmp(req->method, "GET") == 0);
  assert(strcmp(req->path, "/index.html") == 0);
  assert(strcmp(req->version, "HTTP/1.1") == 0);

  free_http_request(req);
  printf("✓ Simple GET request test passed\n\n");
}

void test_post_request_with_body() {
  printf("Testing POST request with body...\n");

  char *request = "POST /api/users HTTP/1.1\r\n"
                  "Host: localhost:8080\r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: 25\r\n"
                  "\r\n"
                  "{\"name\":\"John\",\"age\":30}";

  http_request *req = parse_request(request);
  print_request(req);

  assert(req != NULL);
  assert(strcmp(req->method, "POST") == 0);
  assert(strcmp(req->path, "/api/users") == 0);
  assert(req->body != NULL);

  free_http_request(req);
  printf("✓ POST request with body test passed\n\n");
}

void test_malformed_request() {
  printf("Testing malformed request...\n");

  char *request = "INVALID REQUEST";

  http_request *req = parse_request(request);
  print_request(req);

  // Should handle gracefully and return NULL or partial data
  if (req != NULL) {
    free_http_request(req);
  }
  printf("✓ Malformed request test completed\n\n");
}

void test_empty_request() {
  printf("Testing empty request...\n");

  char *request = "";

  http_request *req = parse_request(request);
  print_request(req);

  if (req != NULL) {
    free_http_request(req);
  }
  printf("✓ Empty request test completed\n\n");
}

void test_headers_only() {
  printf("Testing request with multiple headers...\n");

  char *request = "GET /test HTTP/1.1\r\n"
                  "Host: example.com\r\n"
                  "User-Agent: Mozilla/5.0\r\n"
                  "Accept: text/html\r\n"
                  "Accept-Language: en-US\r\n"
                  "Connection: keep-alive\r\n"
                  "\r\n";

  http_request *req = parse_request(request);
  print_request(req);

  assert(req != NULL);
  assert(strcmp(req->method, "GET") == 0);

  // Test specific headers
  char *host = (char *)ht_get(req->headers, "Host");
  char *user_agent = (char *)ht_get(req->headers, "User-Agent");

  if (host)
    printf("Found Host header: %s\n", host);
  if (user_agent)
    printf("Found User-Agent header: %s\n", user_agent);

  free_http_request(req);
  printf("✓ Multiple headers test completed\n\n");
}

int main() {
  printf("HTTP Parser Test Suite\n");
  printf("=====================\n\n");

  test_simple_get_request();
  test_post_request_with_body();
  test_headers_only();
  test_malformed_request();
  test_empty_request();

  printf("All tests completed!\n");
  return 0;
}
