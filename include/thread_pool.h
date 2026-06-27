#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

#define POLICY_RR  0
#define POLICY_RS  1
#define POLICY_LQS 2
#define POLICY_AS  3

typedef struct task {
    void (*function)(void *);
    void *arg;
} task_t;

typedef struct thread_pool thread_pool_t;

thread_pool_t *thread_pool_init(int num_threads, int scheduler_type, int steal_half);
void thread_pool_submit(thread_pool_t *tp, void (*fn)(void *), void *arg);
void thread_pool_free(thread_pool_t *tp);

#endif
