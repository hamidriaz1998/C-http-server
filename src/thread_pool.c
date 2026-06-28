#define _GNU_SOURCE
#include "../include/thread_pool.h"
#include "../include/deque.h"
#include "../include/metrics.h"
#include "../include/queue.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define AS_RING_SIZE 256

typedef struct worker_thread {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cvar;
    int id;
    struct thread_pool *pool;
    Queue *task_queue;
    deque_t *task_deque;
    worker_metrics_t metrics;
    uint64_t latency_ring[AS_RING_SIZE];
    int latency_ring_idx;
    int latency_ring_count;
} worker_thread_t;

struct thread_pool {
    worker_thread_t *workers;
    int num_threads;
    volatile bool shutdown;
    int scheduler_type;
    int steal_half;
    int next_worker;
    pthread_mutex_t rr_lock;
    deque_t *main_deque;
};

static inline uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#define AS_TARGET_NS 1000000

static int compare_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint64_t compute_p99(worker_thread_t *w) {
    int n = w->latency_ring_count < AS_RING_SIZE ? w->latency_ring_count : AS_RING_SIZE;
    if (n < 10) return 0;
    uint64_t sorted[n];
    int idx = w->latency_ring_idx;
    for (int i = 0; i < n; i++) {
        int pos = (idx - n + i) % AS_RING_SIZE;
        if (pos < 0) pos += AS_RING_SIZE;
        sorted[i] = w->latency_ring[pos];
    }
    qsort(sorted, n, sizeof(uint64_t), compare_u64);
    return sorted[(int)(n * 0.99)];
}

static deque_t *victim_deque(thread_pool_t *tp, int idx) {
    if (idx == tp->num_threads)
        return tp->main_deque;
    return tp->workers[idx].task_deque;
}

static int ws_do_steal(worker_thread_t *w, thread_pool_t *tp, int max_tasks) {
    int num_victims = tp->num_threads + 1;
    int victim;

    if (tp->scheduler_type == POLICY_LQS) {
        victim = -1;
        int max_sz = 0;
        for (int i = 0; i < num_victims; i++) {
            if (i == w->id) continue;
            int sz = deque_size(victim_deque(tp, i));
            if (sz > max_sz) { max_sz = sz; victim = i; }
        }
    } else {
        victim = rand() % num_victims;
        if (victim == w->id) victim = (victim + 1) % num_victims;
    }
    if (victim < 0 || victim >= num_victims) return 0;

    deque_t *vd = victim_deque(tp, victim);
    int sz = deque_size(vd);
    if (sz == 0) return 0;

    int count = tp->steal_half ? (sz / 2) : 1;
    if (count < 1) count = 1;
    if (count > max_tasks) count = max_tasks;

    int stolen = 0;
    for (int i = 0; i < count; i++) {
        task_t *t = deque_steal(vd);
        if (!t) break;
        metrics_record_steal(&w->metrics, 1);
        deque_push_bottom(w->task_deque, t);
        stolen++;
    }
    return stolen;
}

static void *rr_worker_loop(void *arg);
static void *ws_worker_loop(void *arg);

static void *rr_worker_loop(void *arg) {
    worker_thread_t *w = (worker_thread_t *)arg;
    thread_pool_t *tp = w->pool;

    for (;;) {
        pthread_mutex_lock(&w->lock);
        while (queue_isempty(w->task_queue) && !tp->shutdown)
            pthread_cond_wait(&w->cvar, &w->lock);

        if (tp->shutdown && queue_isempty(w->task_queue)) {
            pthread_mutex_unlock(&w->lock);
            pthread_exit(NULL);
        }

        task_t *task = dequeue(w->task_queue);
        pthread_mutex_unlock(&w->lock);

        if (task) {
            uint64_t start = ns_now();
            task->function(task->arg);
            uint64_t end = ns_now();
            metrics_record_task(&w->metrics, start, end);
            free(task);
        }
    }
    return NULL;
}

static void ws_sleep(int us) {
    struct timespec ts = { .tv_sec = us / 1000000, .tv_nsec = (us % 1000000) * 1000 };
    nanosleep(&ts, NULL);
}

static void *ws_worker_loop(void *arg) {
    worker_thread_t *w = (worker_thread_t *)arg;
    thread_pool_t *tp = w->pool;

    while (!tp->shutdown) {
        task_t *task = deque_pop_bottom(w->task_deque);
        if (!task) {
            uint64_t idle_start = ns_now();
            ws_do_steal(w, tp, 8);
            uint64_t idle_end = ns_now();

            task = deque_pop_bottom(w->task_deque);
            if (!task) {
                atomic_fetch_add(&w->metrics.idle_ns, idle_end - idle_start);
                if (tp->scheduler_type == POLICY_AS && w->latency_ring_count >= 10) {
                    uint64_t p99 = compute_p99(w);
                    int slp = p99 < AS_TARGET_NS / 2 ? 100 :
                              p99 < AS_TARGET_NS ? 50 :
                              p99 < AS_TARGET_NS * 3 ? 20 : 10;
                    ws_sleep(slp);
                } else {
                    ws_sleep(50);
                }
            }
        }
        if (task) {
            uint64_t start = ns_now();
            task->function(task->arg);
            uint64_t end = ns_now();
            uint64_t dur = end - start;
            metrics_record_task(&w->metrics, start, end);
            if (tp->scheduler_type == POLICY_AS) {
                if (w->latency_ring_count < AS_RING_SIZE) w->latency_ring_count++;
                w->latency_ring[w->latency_ring_idx] = dur;
                w->latency_ring_idx = (w->latency_ring_idx + 1) % AS_RING_SIZE;
            }
            free(task);
        }
    }
    return NULL;
}

thread_pool_t *thread_pool_init(int num_threads, int scheduler_type, int steal_half) {
    if (num_threads <= 0) return NULL;

    thread_pool_t *tp = malloc(sizeof(thread_pool_t));
    if (!tp) return NULL;

    tp->shutdown = false;
    tp->num_threads = num_threads;
    tp->scheduler_type = scheduler_type;
    tp->steal_half = steal_half;
    tp->next_worker = 0;
    tp->main_deque = NULL;
    if (pthread_mutex_init(&tp->rr_lock, NULL) != 0) {
        free(tp);
        return NULL;
    }

    if (scheduler_type != POLICY_RR) {
        tp->main_deque = deque_init();
        if (!tp->main_deque) {
            pthread_mutex_destroy(&tp->rr_lock);
            free(tp);
            return NULL;
        }
    }

    tp->workers = malloc(sizeof(worker_thread_t) * num_threads);
    if (!tp->workers) {
        pthread_mutex_destroy(&tp->rr_lock);
        free(tp);
        return NULL;
    }

    int inited = 0;
    for (int i = 0; i < num_threads; i++) {
        worker_thread_t *w = &tp->workers[i];
        w->id = i;
        w->pool = tp;
        w->task_queue = NULL;
        w->task_deque = NULL;
        w->latency_ring_idx = 0;
        w->latency_ring_count = 0;
        metrics_init(&w->metrics);

        if (pthread_mutex_init(&w->lock, NULL) != 0) break;
        if (pthread_cond_init(&w->cvar, NULL) != 0) {
            pthread_mutex_destroy(&w->lock);
            break;
        }

        if (scheduler_type == POLICY_RR) {
            w->task_queue = queue_init();
            if (!w->task_queue) {
                pthread_cond_destroy(&w->cvar);
                pthread_mutex_destroy(&w->lock);
                break;
            }
        } else {
            w->task_deque = deque_init();
            if (!w->task_deque) {
                pthread_cond_destroy(&w->cvar);
                pthread_mutex_destroy(&w->lock);
                break;
            }
        }
        inited++;
    }

    if (inited != num_threads) {
        for (int j = 0; j < inited; j++) {
            worker_thread_t *w = &tp->workers[j];
            if (w->task_queue) queue_free(w->task_queue);
            if (w->task_deque) deque_free(w->task_deque);
            pthread_cond_destroy(&w->cvar);
            pthread_mutex_destroy(&w->lock);
        }
        free(tp->workers);
        pthread_mutex_destroy(&tp->rr_lock);
        free(tp);
        return NULL;
    }

    void *(*start_fn)(void *) = (scheduler_type == POLICY_RR) ? rr_worker_loop : ws_worker_loop;

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&tp->workers[i].thread, NULL, start_fn, &tp->workers[i]) != 0) {
            tp->shutdown = true;
            for (int k = 0; k < num_threads; k++) {
                pthread_mutex_lock(&tp->workers[k].lock);
                pthread_cond_broadcast(&tp->workers[k].cvar);
                pthread_mutex_unlock(&tp->workers[k].lock);
            }
            for (int j = 0; j < i; j++)
                pthread_join(tp->workers[j].thread, NULL);
            for (int j = 0; j < num_threads; j++) {
                worker_thread_t *w = &tp->workers[j];
                if (w->task_queue) queue_free(w->task_queue);
                if (w->task_deque) deque_free(w->task_deque);
                pthread_cond_destroy(&w->cvar);
                pthread_mutex_destroy(&w->lock);
            }
            free(tp->workers);
            pthread_mutex_destroy(&tp->rr_lock);
            free(tp);
            return NULL;
        }
    }

    return tp;
}

void thread_pool_submit(thread_pool_t *tp, void (*fn)(void *), void *arg) {
    if (!tp || !fn) return;

    task_t *task = malloc(sizeof(task_t));
    if (!task) return;
    task->function = fn;
    task->arg = arg;

    if (tp->scheduler_type == POLICY_RR) {
        pthread_mutex_lock(&tp->rr_lock);
        int target = tp->next_worker;
        tp->next_worker = (tp->next_worker + 1) % tp->num_threads;
        pthread_mutex_unlock(&tp->rr_lock);

        worker_thread_t *w = &tp->workers[target];
        pthread_mutex_lock(&w->lock);
        enqueue(w->task_queue, task);
        pthread_cond_signal(&w->cvar);
        pthread_mutex_unlock(&w->lock);
    } else {
        deque_push_bottom(tp->main_deque, task);
    }
}

void thread_pool_free(thread_pool_t *tp) {
    if (!tp) return;

    tp->shutdown = true;
    for (int i = 0; i < tp->num_threads; i++) {
        pthread_mutex_lock(&tp->workers[i].lock);
        pthread_cond_broadcast(&tp->workers[i].cvar);
        pthread_mutex_unlock(&tp->workers[i].lock);
    }

    for (int i = 0; i < tp->num_threads; i++)
        pthread_join(tp->workers[i].thread, NULL);

    printf("\n=== Worker Metrics ===\n");
    for (int i = 0; i < tp->num_threads; i++) {
        worker_thread_t *w = &tp->workers[i];
        uint64_t done = atomic_load(&w->metrics.tasks_completed);
        uint64_t attempts = atomic_load(&w->metrics.steal_attempts);
        uint64_t succ = atomic_load(&w->metrics.steal_successes);
        uint64_t idle = atomic_load(&w->metrics.idle_ns);
        uint64_t sum = atomic_load(&w->metrics.task_ns_sum);
        uint64_t mx = atomic_load(&w->metrics.task_ns_max);
        printf("  Worker %d: %lu tasks, %lu/%lu steals, idle=%lu ms, avg=%.1f us, max=%lu us\n",
               i, done, succ, attempts, idle / 1000000,
               done > 0 ? (sum / done) / 1000.0 : 0.0, mx / 1000);
    }

    for (int i = 0; i < tp->num_threads; i++) {
        worker_thread_t *w = &tp->workers[i];
        if (w->task_queue) queue_free(w->task_queue);
        if (w->task_deque) deque_free(w->task_deque);
        pthread_cond_destroy(&w->cvar);
        pthread_mutex_destroy(&w->lock);
    }

    if (tp->main_deque) deque_free(tp->main_deque);
    free(tp->workers);
    pthread_mutex_destroy(&tp->rr_lock);
    free(tp);
}
