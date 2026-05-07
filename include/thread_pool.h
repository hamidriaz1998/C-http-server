#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "queue.h"
#include <pthread.h>

typedef struct task {
  void (*function)(void *);
  void *arg;
} task_t;

// Forward declaration for worker_thread's pool pointer
struct thread_pool;

typedef struct worker_thread {
  pthread_t thread;
  Queue *task_queue;
  pthread_mutex_t lock;
  pthread_cond_t cvar;
  struct thread_pool *pool;
} worker_thread_t;

typedef struct thread_pool {
  worker_thread_t *workers;
  bool shutdown;
  int num_threads;
  int next_worker;
  pthread_mutex_t rr_lock;
} thread_pool_t;

thread_pool_t *thread_pool_init(int num_threads);
bool thread_pool_add_task(thread_pool_t *thp, void (*fn)(void *), void *arg);
void thread_pool_free(thread_pool_t *thp);
void *thread_worker(void *arg);
#endif
