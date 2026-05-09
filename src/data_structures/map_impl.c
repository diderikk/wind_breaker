#include "session_by_thread_map.h"
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define TABLE_MAX_LOAD 0.75

typedef struct {
  unsigned long thread_id;
  int session_id;
} entry_t;

typedef struct {
  unsigned int count;
  unsigned int capacity;
  entry_t *entries;
} table_t;

static table_t table = {
    .count = 0,
    .capacity = 0,
    .entries = NULL,
};
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static entry_t *find_entry(entry_t *entries, unsigned long thread_id);
static int grow_capacity();

void destroy_session_by_thread_map() {
  pthread_mutex_lock(&mutex);
  if (table.entries != NULL) {
    free(table.entries);
    table.entries = NULL;
  }
  table.capacity = 0;
  table.count = 0;
  pthread_mutex_unlock(&mutex);
}

int put_session_id_by_thread(int session_id) {
  pthread_mutex_lock(&mutex);

  if (table.count + 1 > table.capacity * TABLE_MAX_LOAD) {
    int result = grow_capacity();
    if (result < 0) {
      pthread_mutex_unlock(&mutex);
      return result;
    }
  }
  unsigned long thread_id = pthread_self();
  entry_t *entry = find_entry(table.entries, thread_id);
  bool is_new_entry = entry->session_id == -1;
  if (session_id == -1)
    table.count -= 1;
  else if (is_new_entry)
    table.count += 1;

  entry->session_id = session_id;
  entry->thread_id = thread_id;
  pthread_mutex_unlock(&mutex);
  return 1;
}

int get_session_id_by_thread() {
  pthread_mutex_lock(&mutex);
  int result = -1;

  if (table.count > 0) {
    result = find_entry(table.entries, pthread_self())->session_id;
  }

  pthread_mutex_unlock(&mutex);
  return result;
}

static entry_t *find_entry(entry_t *entries, unsigned long thread_id) {
  unsigned int index = thread_id % table.capacity;

  while (1) {
    entry_t *entry = &table.entries[index];
    if (entry->thread_id == thread_id || entry->session_id == -1) {
      return entry;
    }

    index = (index + 1) % table.capacity;
  }
}

static int grow_capacity() {
  unsigned int new_capacity;
  if (table.capacity == 0) {
    new_capacity = 16;
  } else {
    new_capacity *= 2;
  }

  entry_t *new_entries = malloc(sizeof(entry_t) * new_capacity);
  if (new_entries == NULL) {
    return -1;
  }

  for (int i = 0; i < new_capacity; i++) {
    new_entries[i].session_id = -1;
    new_entries[i].thread_id = -1;
  }

  for (int i = 0; i < table.capacity; i++) {
    entry_t *entry = &table.entries[i];
    if (entry->session_id == -1)
      continue;

    entry_t *dest = find_entry(new_entries, entry->thread_id);
    dest->thread_id = entry->thread_id;
    dest->session_id = entry->session_id;
  }

  free(table.entries);
  table.entries = new_entries;
  table.capacity = new_capacity;
  return table.capacity;
}
