#define _GNU_SOURCE
#include "../include/hashtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint64_t hash_key(const char *key);
static const char *ht_set_entry(ht_entry *entries, size_t capacity,
                                const char *key, void *value, size_t *plength);
static bool ht_expand(hashtable *table);

hashtable *ht_create() {
  hashtable *table = (hashtable *)malloc(sizeof(hashtable));
  if (table == NULL) {
    return NULL;
  }
  table->length = 0;
  table->capacity = INITIAL_CAPACITY;

  table->entries = calloc(table->capacity, sizeof(ht_entry));
  if (table->entries == NULL) {
    free(table);
    return NULL;
  }
  return table;
}

void ht_destroy(hashtable *table) {
  if (table == NULL) {
    return;
  }
  for (size_t i = 0; i < table->capacity; i++) {
    free((void *)table->entries[i].key);
  }

  free(table->entries);
  free(table);
}

void *ht_get(hashtable *table, const char *key) {
  uint64_t hash = hash_key(key);
  size_t index = (size_t)(hash & (uint64_t)(table->capacity - 1));

  while (table->entries[index].key != NULL) {
    if (strcmp(key, table->entries[index].key) == 0) {
      return table->entries[index].value;
    }
    index++;
    if (index >= table->capacity) {
      index = 0;
    }
  }
  return NULL;
}

const char *ht_set(hashtable *table, const char *key, void *value) {
  if (value == NULL) {
    return NULL;
  }

  if (table->length >= table->capacity / 2) {
    if (!ht_expand(table)) {
      return NULL;
    }
  }
  return ht_set_entry(table->entries, table->capacity, key, value,
                      &table->length);
}

static const char *ht_set_entry(ht_entry *entries, size_t capacity,
                                const char *key, void *value, size_t *plength) {
  uint64_t hash = hash_key(key);
  size_t index = (size_t)(hash & (uint64_t)(capacity - 1));

  while (entries[index].key != NULL) {
    if (strcmp(key, entries[index].key) == 0) {
      entries[index].value = value; // Found key (already exists), update value
      return entries[index].key;
    }
    index++;
    if (index >= capacity) {
      index = 0; // At end of array, wrap around
    }
  }

  // Didn't find the key, allocate + copy if needed
  if (plength != NULL) {
    key = strdup(key);
    if (key == NULL) {
      return NULL;
    }
    (*plength)++;
  }
  entries[index].key = (char *)key;
  entries[index].value = value;

  return key;
}

static bool ht_expand(hashtable *table) {
  size_t new_cap = table->capacity * 2;
  if (new_cap < table->capacity) {
    return false; // Overflow
  }

  ht_entry *new_entry_arr = calloc(new_cap, sizeof(ht_entry));
  if (new_entry_arr == NULL) {
    return false;
  }

  for (size_t i = 0; i < table->capacity; i++) {
    ht_entry entry = table->entries[i];
    if (entry.key != NULL) {
      ht_set_entry(new_entry_arr, new_cap, entry.key, entry.value, NULL);
    }
  }

  free(table->entries);
  table->entries = new_entry_arr;
  table->capacity = new_cap;
  return true;
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const char *key) {
  uint64_t hash = FNV_OFFSET;
  for (const char *p = key; *p; p++) {
    hash ^= (uint64_t)(unsigned char)(*p);
    hash *= FNV_PRIME;
  }
  return hash;
}

hashtable *ht_copy(hashtable *table) {
  hashtable *new_table = malloc(sizeof(hashtable));
  if (new_table == NULL) {
    return NULL;
  }
  *new_table = *table;
  new_table->entries = calloc(new_table->capacity, sizeof(ht_entry));
  if (new_table->entries == NULL) {
    free(new_table);
    return NULL;
  }
  for (size_t i = 0; i < table->capacity; i++) {
    if (table->entries[i].key != NULL) {
      ht_set_entry(new_table->entries, new_table->capacity,
                   table->entries[i].key, table->entries[i].value, NULL);
    }
  }
  return new_table;
}
