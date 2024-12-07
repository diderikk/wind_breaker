#include "session.h"
#include <pthread.h>
#include <stdlib.h>

#define INITIAL_SESSION_SIZE 500

static struct session *session_array = NULL;
static int session_count = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_session_cache() {
  if (session_array != NULL) {
    return;
  }
  // Static array of sessions, should not be resized... Tiger style
  session_array = malloc(sizeof(*session_array) * INITIAL_SESSION_SIZE);
}

// Add a new session to the set
void add_to_session_sync(int related_fd) {
  pthread_mutex_lock(&session_mutex);
  // Ring buffer... could also used modular arithmetic
  if (session_count == INITIAL_SESSION_SIZE) {
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
  pthread_mutex_lock(&session_mutex);
  int found = 0;
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].related_fd == related_fd) {
      found++;
      session_array[i].thread_id = pthread_self();
      break;
    }
  }
  pthread_mutex_unlock(&session_mutex);
  if (!found) {
    // This should never happen
    // If it does, it means we have a session that was not added to the session
    // array This is a bug
    add_thread_to_session(related_fd);
  }
}

void del_from_session_sync(int related_fd) {
  pthread_mutex_lock(&session_mutex);

  int i = 0;
  for (; i < session_count; i++) {
    if (session_array[i].related_fd == related_fd) {
      break;
    }
  }

  for (; i < session_count - 1; i++) {
    session_array[i] = session_array[i + 1];
  }

  session_count--;

  pthread_mutex_unlock(&session_mutex);
}
