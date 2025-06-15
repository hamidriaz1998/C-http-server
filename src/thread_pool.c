#include "../include/thread_pool.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdlib.h>

thread_pool_t *thread_pool_init(int num_threads) {

  // Initialize thread pool struct
  thread_pool_t *thp = (thread_pool_t *)malloc(sizeof(thread_pool_t));
  if (thp == NULL){
      return NULL;
  }
  
  // Initialize thread array
  thp->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
  if (NULL == thp->threads) {
    free(thp);
    return NULL;
  }

  // Initialize queue
  thp->task_queue = queue_init();
  if (NULL == thp->task_queue) {
    free(thp->threads);
    free(thp);
    return NULL;
  }

  // Initialize locks
  if (pthread_mutex_init(&thp->lock, NULL) != 0 ||
      pthread_cond_init(&thp->task_added_c, NULL) != 0) {
    free(thp->threads);
    queue_free(thp->task_queue);
    free(thp);
    return NULL;
  }

  thp->shutdown = false;
  thp->num_threads = num_threads;

  // Create threads
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&thp->threads[i], NULL, thread_worker, (void *)thp) !=
        0) {
      thp->shutdown = true;
      pthread_cond_broadcast(&thp->task_added_c);

      for (int j = 0; j < i; j++) {
        pthread_join(thp->threads[j], NULL);
      }

      pthread_mutex_destroy(&thp->lock);
      pthread_cond_destroy(&thp->task_added_c);
      free(thp->threads);
      queue_free(thp->task_queue);
      free(thp);
      return NULL;
    }
  }
  return thp;
}

bool thread_pool_add_task(thread_pool_t *thp, void (*fn)(void *), void *arg) {
  if (!thp || !fn) {
    return false;
  }

  task_t *task = (task_t *)malloc(sizeof(task_t));
  if (task == NULL) {
    return false;
  }
  task->function = fn;
  task->arg = arg;

  pthread_mutex_lock(&thp->lock);
  enqueue(thp->task_queue, (void *)task);
  pthread_cond_signal(&thp->task_added_c);
  pthread_mutex_unlock(&thp->lock);

  return true;
}

void thread_pool_free(thread_pool_t *thp) {
  if (!thp) {
    return;
  }

  pthread_mutex_lock(&thp->lock);
  thp->shutdown = true;
  pthread_cond_broadcast(&thp->task_added_c);
  pthread_mutex_unlock(&thp->lock);

  for (int i = 0; i < thp->num_threads; i++) {
    pthread_join(thp->threads[i], NULL);
  }

  pthread_mutex_destroy(&thp->lock);
  pthread_cond_destroy(&thp->task_added_c);
  queue_free(thp->task_queue);
  free(thp->threads);
  free(thp);
}

void *thread_worker(void *arg) {
  thread_pool_t *thp = (thread_pool_t *)arg;
  task_t *task;

  for (;;) {
    pthread_mutex_lock(&thp->lock);
    while (queue_isempty(thp->task_queue) && !thp->shutdown) {
      pthread_cond_wait(&thp->task_added_c, &thp->lock);
    }

    if (thp->shutdown && queue_isempty(thp->task_queue)) {
      pthread_mutex_unlock(&thp->lock);
      pthread_exit(NULL);
    }

    task = (task_t *)dequeue(thp->task_queue);
    pthread_mutex_unlock(&thp->lock);

    task->function(task->arg);

    free(task);
  }
  return NULL;
}
