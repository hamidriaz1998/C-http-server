#ifndef CLI_H
#define CLI_H

#include "thread_pool.h"

typedef struct {
    int port;
    int threads;
    const char *root;
    int scheduler;
    int steal_half;
} config_t;

void parse_args(int argc, char *argv[], config_t *cfg);
void print_usage(const char *prog);

#endif
