#ifndef UTILS_H
#define UTILS_H

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

const char *get_mime_type(const char *path);
int resolve_request_path(const char *url, const char *doc_root, char *out,
                         size_t out_size);

#endif
