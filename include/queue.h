#ifndef QUEUE_H
#define QUEUE_H

#include<stdbool.h>

typedef struct queue_node {
  void *data;
  struct queue_node *next;
} queue_node_t;

typedef struct Queue {
  queue_node_t *head;
  queue_node_t *tail;
  int size;
} Queue;

void queue_init(Queue *q);
bool enqueue(Queue *q, void *data);
void *dequeue(Queue *q);
bool queue_isempty(Queue *q);
void queue_free(Queue *q);

#endif
