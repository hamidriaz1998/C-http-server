#include "../include/queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

Queue *queue_init() {
  Queue *q = (Queue *)malloc(sizeof(Queue));
  if (q == NULL) {
    return NULL;
  }
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;

  return q;
}

bool enqueue(Queue *q, void *data) {
  queue_node_t *new_node = (queue_node_t *)malloc(sizeof(queue_node_t));
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
  if (q == NULL || queue_isempty(q)) {
    return NULL;
  }

  queue_node_t *head = q->head;
  q->head = head->next;

  if (q->head == NULL) {
    q->tail = NULL;
  }

  q->size--;
  void *data = head->data;
  free(head);
  return data;
}

bool queue_isempty(Queue *q) {
  if (q)
    return q->head == NULL;
  return false;
}

void queue_free(Queue *q) {
  if (q == NULL) {
    return;
  }
  queue_node_t *current = q->head;
  while (current != NULL) {
    queue_node_t *next = current->next;
    free(current);
    current = next;
  }
  free(q);
}
