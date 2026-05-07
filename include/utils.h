#ifndef UTILS_H
#define UTILS_H

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MAX_PATH_SEGMENTS 256
#define ASCII_CTRL_START 0x20
#define ASCII_DEL 0x7F

const char *get_mime_type(const char *path);
int resolve_request_path(const char *url, const char *doc_root, char *out,
                         size_t out_size);

#endif
