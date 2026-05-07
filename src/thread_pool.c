#include "../include/thread_pool.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

thread_pool_t *thread_pool_init(int num_threads) {
  if (num_threads <= 0)
    return NULL;

  thread_pool_t *thp = malloc(sizeof(thread_pool_t));
  if (!thp)
    return NULL;

  thp->shutdown = false;
  thp->num_threads = num_threads;
  thp->next_worker = 0;
  if (pthread_mutex_init(&thp->rr_lock, NULL) != 0) {
    free(thp);
    return NULL;
  }

  thp->workers = malloc(sizeof(worker_thread_t) * num_threads);
  if (!thp->workers) {
    pthread_mutex_destroy(&thp->rr_lock);
    free(thp);
    return NULL;
  }

  // Initialize each worker
  int initialized_workers = 0;
  for (int i = 0; i < num_threads; i++) {
    worker_thread_t *w = &thp->workers[i];
    w->pool = thp;
    w->task_queue = queue_init();
    if (!w->task_queue)
      break;

    if (pthread_mutex_init(&w->lock, NULL) != 0) {
      queue_free(w->task_queue);
      break;
    }

    if (pthread_cond_init(&w->cvar, NULL) != 0) {
      pthread_mutex_destroy(&w->lock);
      queue_free(w->task_queue);
      break;
    }
    initialized_workers++;
  }

  if (initialized_workers != num_threads) {
    for (int j = 0; j < initialized_workers; j++) {
      worker_thread_t *w = &thp->workers[j];
      pthread_mutex_destroy(&w->lock);
      pthread_cond_destroy(&w->cvar);
      queue_free(w->task_queue);
    }
    free(thp->workers);
    pthread_mutex_destroy(&thp->rr_lock);
    free(thp);
    return NULL;
  }

  // Create worker threads
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&thp->workers[i].thread, NULL, thread_worker,
                       &thp->workers[i]) != 0) {
      thp->shutdown = true;
      for (int k = 0; k < num_threads; k++) {
        pthread_mutex_lock(&thp->workers[k].lock);
        pthread_cond_broadcast(&thp->workers[k].cvar);
        pthread_mutex_unlock(&thp->workers[k].lock);
      }
      for (int j = 0; j < i; j++) {
        pthread_join(thp->workers[j].thread, NULL);
      }
      for (int j = 0; j < num_threads; j++) {
        worker_thread_t *w = &thp->workers[j];
        pthread_mutex_destroy(&w->lock);
        pthread_cond_destroy(&w->cvar);
        queue_free(w->task_queue);
      }
      free(thp->workers);
      pthread_mutex_destroy(&thp->rr_lock);
      free(thp);
      return NULL;
    }
  }

  return thp;
}

bool thread_pool_add_task(thread_pool_t *thp, void (*fn)(void *), void *arg) {
  if (!thp || !fn)
    return false;

  task_t *task = malloc(sizeof(task_t));
  if (!task)
    return false;
  task->function = fn;
  task->arg = arg;

  pthread_mutex_lock(&thp->rr_lock);
  int target = thp->next_worker;
  thp->next_worker = (thp->next_worker + 1) % thp->num_threads;
  pthread_mutex_unlock(&thp->rr_lock);

  worker_thread_t *w = &thp->workers[target];
  pthread_mutex_lock(&w->lock);
  enqueue(w->task_queue, task);
  pthread_cond_signal(&w->cvar);
  pthread_mutex_unlock(&w->lock);

  return true;
}

void thread_pool_free(thread_pool_t *thp) {
  if (!thp)
    return;

  thp->shutdown = true;
  for (int i = 0; i < thp->num_threads; i++) {
    worker_thread_t *w = &thp->workers[i];
    pthread_mutex_lock(&w->lock);
    pthread_cond_broadcast(&w->cvar);
    pthread_mutex_unlock(&w->lock);
  }

  for (int i = 0; i < thp->num_threads; i++) {
    pthread_join(thp->workers[i].thread, NULL);
  }

  for (int i = 0; i < thp->num_threads; i++) {
    worker_thread_t *w = &thp->workers[i];
    pthread_mutex_destroy(&w->lock);
    pthread_cond_destroy(&w->cvar);
    queue_free(w->task_queue);
  }

  free(thp->workers);
  pthread_mutex_destroy(&thp->rr_lock);
  free(thp);
}

void *thread_worker(void *arg) {
  worker_thread_t *worker = (worker_thread_t *)arg;
  thread_pool_t *thp = worker->pool;
  task_t *task;

  for (;;) {
    pthread_mutex_lock(&worker->lock);
    while (queue_isempty(worker->task_queue) && !thp->shutdown) {
      pthread_cond_wait(&worker->cvar, &worker->lock);
    }

    if (thp->shutdown && queue_isempty(worker->task_queue)) {
      pthread_mutex_unlock(&worker->lock);
      pthread_exit(NULL);
    }

    task = dequeue(worker->task_queue);
    pthread_mutex_unlock(&worker->lock);

    if (task) {
      task->function(task->arg);
      free(task);
    }
  }
  return NULL;
}
