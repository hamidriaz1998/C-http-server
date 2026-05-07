#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define INITIAL_CAPACITY 10
#define HT_LOAD_FACTOR_DENOM 2
#define HT_EXPANSION_FACTOR 2

typedef struct {
  const char *key;
  void *value;
} ht_entry;

typedef struct {
  ht_entry *entries;
  size_t capacity;
  size_t length;
} hashtable;

hashtable *ht_create();
void ht_destroy(hashtable *ht);
void *ht_get(hashtable *table, const char *key);
const char *ht_set(hashtable *table, const char *key, void *value);
hashtable *ht_copy(hashtable *table);
#endif
