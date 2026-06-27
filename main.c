#define _GNU_SOURCE
#include "include/cli.h"
#include "include/network.h"
#include "include/thread_handler.h"
#include "include/thread_pool.h"
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#define MAX_EVENTS 64

static volatile sig_atomic_t shutdown_flag = 0;

int main(int argc, char *argv[]) {
    config_t cfg;
    parse_args(argc, argv, &cfg);

    set_doc_root(cfg.root);

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGQUIT);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    signal(SIGPIPE, SIG_IGN);

    int sfd = signalfd(-1, &sigset, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1) {
        perror("signalfd");
        exit(EXIT_FAILURE);
    }

    int listen_fd = create_listener(cfg.port);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to create listener on port %d\n", cfg.port);
        exit(EXIT_FAILURE);
    }

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        perror("epoll_create1");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl listen");
        close(epfd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) == -1) {
        perror("epoll_ctl signalfd");
        close(epfd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    thread_pool_t *tp = thread_pool_init(cfg.threads, cfg.scheduler, cfg.steal_half);
    if (!tp) {
        fprintf(stderr, "Failed to initialize thread pool\n");
        close(epfd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d (%d workers, scheduler=%s)\n",
           cfg.port, cfg.threads,
           cfg.scheduler == POLICY_RR ? "rr" :
           cfg.scheduler == POLICY_RS ? "rs" :
           cfg.scheduler == POLICY_LQS ? "lqs" : "as");
    printf("Document root: %s\n", cfg.root);
    printf("Press Ctrl+C to stop\n");

    struct epoll_event events[MAX_EVENTS];

    while (!shutdown_flag) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, 1000);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == sfd) {
                struct signalfd_siginfo fdsi;
                read(sfd, &fdsi, sizeof(fdsi));
                shutdown_flag = 1;
                break;
            }

            if (events[i].data.fd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int client_fd = accept4(listen_fd, (struct sockaddr *)&client_addr,
                                        &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (client_fd == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                        perror("accept4");
                    continue;
                }

                int *fd_ptr = malloc(sizeof(int));
                if (!fd_ptr) {
                    close(client_fd);
                    continue;
                }
                *fd_ptr = client_fd;
                thread_pool_submit(tp, connection_handler, fd_ptr);
            }
        }
    }

    printf("\nShutting down...\n");
    thread_pool_free(tp);
    close(epfd);
    close(listen_fd);
    close(sfd);
    return 0;
}
