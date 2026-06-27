#include "../include/cli.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct option long_options[] = {
    {"port",        required_argument, 0, 'p'},
    {"root",        required_argument, 0, 'r'},
    {"threads",     required_argument, 0, 't'},
    {"scheduler",   required_argument, 0, 's'},
    {"steal-batch", required_argument, 0, 'b'},
    {"help",        no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  -p, --port <port>          Port number (default: 9000)\n");
    fprintf(stderr, "  -r, --root <path>          Document root (default: ./public)\n");
    fprintf(stderr, "  -t, --threads <n>          Number of worker threads (default: 4)\n");
    fprintf(stderr, "  -s, --scheduler <type>     Scheduler: rr, rs, lqs, as (default: rr)\n");
    fprintf(stderr, "  -b, --steal-batch <mode>   Steal batch: 1 or half (default: 1)\n");
    fprintf(stderr, "  -h, --help                 Show this help\n");
}

static int parse_scheduler(const char *str) {
    if (strcmp(str, "rr") == 0)  return POLICY_RR;
    if (strcmp(str, "rs") == 0)  return POLICY_RS;
    if (strcmp(str, "lqs") == 0) return POLICY_LQS;
    if (strcmp(str, "as") == 0)  return POLICY_AS;
    return -1;
}

void parse_args(int argc, char *argv[], config_t *cfg) {
    cfg->port = 9000;
    cfg->threads = 4;
    cfg->root = "./public";
    cfg->scheduler = POLICY_RR;
    cfg->steal_half = 0;

    int opt;
    while ((opt = getopt_long(argc, argv, "p:r:t:s:b:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'p':
            cfg->port = atoi(optarg);
            break;
        case 'r':
            cfg->root = optarg;
            break;
        case 't':
            cfg->threads = atoi(optarg);
            break;
        case 's':
            cfg->scheduler = parse_scheduler(optarg);
            if (cfg->scheduler < 0) {
                fprintf(stderr, "Unknown scheduler: %s\n", optarg);
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'b':
            if (strcmp(optarg, "half") == 0)
                cfg->steal_half = 1;
            else
                cfg->steal_half = 0;
            break;
        case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}
