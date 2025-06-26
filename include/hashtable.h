#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define INTIAL_CAPACITY 5

typedef struct {
  const char *key;
  void *value;
} ht_entry;

typedef struct {
  ht_entry *entries;
  size_t capacity;
  size_t length;
} ht;

ht *ht_create();
void ht_destroy(ht *ht);
void *ht_get(ht *table, const char *key);
const char *ht_set(ht *table, const char *key, void *value);
static const char *ht_set_entry(ht_entry *entries, size_t capacity,
                                const char *key, void *value, size_t *plength);
static bool ht_expand(ht *table);
static uint64_t hash_key(const char *key);
#endif
