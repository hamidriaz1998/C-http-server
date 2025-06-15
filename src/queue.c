#include "../include/queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void queue_init(Queue *q) {
  if (q != NULL) {
    return;
  }
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
}

bool enqueue(Queue *q, void *data) {
  QueueNode *new_node = (QueueNode *)malloc(sizeof(QueueNode));
  if (new_node == NULL) {
    return false;
  }

  new_node->data = data;
  new_node->next = NULL;

  if (q->tail == NULL) {
    q->head = new_node;
  } else {
    q->tail->next = new_node;
  }
  q->tail = new_node;
  q->size++;
  return true;
}

void *dequeue(Queue *q) {
  if (queue_isempty(q)) {
    return NULL;
  }

  QueueNode *head = q->head;
  q->head = head->next;
  q->size--;

  void *data = head->data;

  // Free QueueNode
  free(head);

  return data;
}

bool queue_isempty(Queue *q) { return q->head == NULL; }

void queue_free(Queue *q) {
  if (queue_isempty(q)) {
    return;
  }
  QueueNode *current = q->head;
  while (current != NULL) {
    QueueNode *next = current->next;
    free(current);
    current = next;
  }
  free(q);
}
