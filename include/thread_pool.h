#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "queue.h"
#include <pthread.h>

typedef struct task {
  void (*function)(void *);
  void *arg;
} task_t;

typedef struct thread_pool {
  pthread_t *threads;
  Queue *task_queue;

  pthread_mutex_t lock;
  pthread_cond_t task_added_c;
  
  bool shutdown;
  int num_threads;
} thread_pool_t;

thread_pool_t *thread_pool_init(int num_threads);
bool thread_pool_add_task(thread_pool_t *thp, void (*fn)(void *), void *arg);
void thread_pool_free(thread_pool_t *thp);
void *thread_worker(void *arg);
#endif
