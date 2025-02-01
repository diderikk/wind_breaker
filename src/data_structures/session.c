#include "session.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static struct session *session_array = NULL;
static int max_size = 0;
static int session_count = 0;
static int session_last_in_index = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

static void assert_(const char *file, int line, const char *func,
                    const char *msg) {
  fprintf(stdout, "Assertion failed: %s:%d: %s: %s\n", file, line, func, msg);
  exit(EXIT_FAILURE);
}
#define assert(expr)                                                           \
  ((void)((expr) || (assert_(__FILE__, __LINE__, __func__, #expr), 0)))

int init_session_cache(int _max_size) {
  assert(_max_size > 0);
  assert(session_array == NULL);
  // Static array of sessions, should not be resized... Tiger style
  session_array = calloc(_max_size, sizeof(*session_array));
  assert(session_array != NULL);
  max_size = _max_size;
  return 0;
}

void destroy_session_cache() {
  assert(session_array != NULL);
  free(session_array);
  session_count = 0;
  session_last_in_index = 0;
  session_array = NULL;
}

void add_to_session_sync(int related_fd) {
  assert(session_array != NULL);
  assert(related_fd > 0);
  pthread_mutex_lock(&session_mutex);
  // Ring buffer... could also used modular arithmetic
  int next = session_count < max_size ? session_count : session_last_in_index;
  assert(next < max_size);

  session_array[next].id = rand();
  session_array[next].related_fd = related_fd;

  if (session_count < max_size) {
    session_count++;
  } else {
    session_last_in_index = (session_last_in_index + 1) % max_size;
  }
  pthread_mutex_unlock(&session_mutex);
}

// Get the session for a given thread id
struct session *get_session_for_thread() {
  if (session_array == NULL) {
    return NULL;
  }
  pthread_mutex_lock(&session_mutex);
  unsigned long thread_id = pthread_self();
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].thread_id == thread_id) {
      pthread_mutex_unlock(&session_mutex);
      return &session_array[i];
    }
  }
  pthread_mutex_unlock(&session_mutex);
  return NULL;
}

// After a thread is done with a session, set the thread_id to 0 (unassign it)
void remove_thread_from_session() {
  assert(session_array != NULL);
  pthread_mutex_lock(&session_mutex);
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].thread_id == pthread_self()) {
      session_array[i].thread_id = 0;
      break;
    }
  }
  pthread_mutex_unlock(&session_mutex);
}

// Assign a thread to a session.
// Caused by having a worker queue, and we need to know which session a worker
// is handling
void add_thread_to_session(int related_fd) {
  assert(session_array != NULL);
  pthread_mutex_lock(&session_mutex);
  int found = 0;
  for (int i = 0; i < session_count; i++) {
    assert(session_array[i].thread_id != pthread_self());
    if (session_array[i].related_fd == related_fd) {
      found = 1;
      session_array[i].thread_id = pthread_self();
    }
  }
  pthread_mutex_unlock(&session_mutex);
  assert(found == 1);
}

void del_from_session_sync(int related_fd) {
  assert(session_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);
  pthread_mutex_lock(&session_mutex);

  int found = 0;
  int i = 0;
  for (; i < session_count; i++) {
    if (session_array[i].related_fd == related_fd) {
      found = 1;
      break;
    }
  }

  if (found == 1) {
    for (; i < session_count - 1; i++) {
      session_array[i].related_fd = session_array[i + 1].related_fd;
      session_array[i].id = session_array[i + 1].id;
      session_array[i].thread_id = session_array[i + 1].thread_id;
      session_array[i + 1].thread_id = 0;
      session_array[i + 1].related_fd = 0;
      session_array[i + 1].id = 0;
    }

    if (session_count > 0)
      session_count--;

    if (i < session_last_in_index)
      session_last_in_index--;
  }

  pthread_mutex_unlock(&session_mutex);
}
