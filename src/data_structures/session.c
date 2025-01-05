#include "session.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

static struct session *session_array = NULL;
static int max_size = 0;
static int session_count = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_session_cache(int _max_size) {
  if (session_array != NULL) {
    return -1;
  }
  // Static array of sessions, should not be resized... Tiger style
  session_array = calloc(_max_size, sizeof(*session_array));
  assert(session_array != NULL);
  max_size = _max_size;
  return 0;
}

// Add a new session to the set
void add_to_session_sync(int related_fd) {
  assert(session_array != NULL);
  pthread_mutex_lock(&session_mutex);
  // Ring buffer... could also used modular arithmetic
  if (session_count == max_size) {
    session_count = 0;
  }

  session_array[session_count].id = rand();
  session_array[session_count].related_fd = related_fd;
  // Should only be called from the main thread (thread that polls for new
  // connections)
  // session_array[session_count].thread_id = pthread_self();

  session_count++;
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
    if (session_array[i].related_fd == related_fd) {
      found = 1;
      session_array[i].thread_id = pthread_self();
      break;
    }
  }
  pthread_mutex_unlock(&session_mutex);
  assert(found == 1);
}

void del_from_session_sync(int related_fd) {
  assert(session_array != NULL);
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
      session_array[i] = session_array[i + 1];
    }

    if (session_count > 0)
      session_count--;
  }

  pthread_mutex_unlock(&session_mutex);
}
