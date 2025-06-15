#ifndef QUEUE_H
#define QUEUE_H

#include<stdbool.h>

typedef struct queueNode {
  void *data;
  struct queueNode *next;
} QueueNode;

typedef struct Queue {
  QueueNode *head;
  QueueNode *tail;
  int size;
} Queue;

void queue_init(Queue *q);
bool enqueue(Queue *q, void *data);
void *dequeue(Queue *q);
bool queue_isempty(Queue *q);
void queue_free(Queue *q);

#endif
