#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define INITIAL_CAPACITY 5

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
static const char *ht_set_entry(ht_entry *entries, size_t capacity,
                                const char *key, void *value, size_t *plength);
static bool ht_expand(hashtable *table);
static uint64_t hash_key(const char *key);
#endif
