#include "session.h"
#include "../static.h"
#include <openssl/err.h>

static struct session *session_array = NULL;
static SSL **ssl_array = NULL;
static BIO **bio_array = NULL;
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

int init_session_cache(int _max_size, SSL_CTX *ctx) {
  assert(_max_size > 0);
  assert(session_array == NULL);
  // Static array of sessions, should not be resized... Tiger style
  session_array = calloc(_max_size, sizeof(*session_array));
  assert(session_array != NULL);
  max_size = _max_size;

  bio_array = malloc(max_size * sizeof(BIO *));
  assert(bio_array != NULL);

  for (int i = 0; i < max_size; i++) {
    bio_array[i] = BIO_new(BIO_s_socket());
    BIO_set_nbio(bio_array[i], 1);
    assert(bio_array[i] != NULL);
  }

  if (ctx != NULL) {
    ssl_array = malloc(max_size * sizeof(SSL *));
    assert(ssl_array != NULL);

    for (int i = 0; i < max_size; i++) {
      ssl_array[i] = SSL_new(ctx);
      assert(ssl_array[i] != NULL);
    }
  }

  return 0;
}

void destroy_session_cache() {
  assert(session_array != NULL);
  assert(bio_array != NULL);

  free(session_array);
  session_count = 0;
  session_last_in_index = 0;
  session_array = NULL;

  for (int i = 0; i < max_size; i++) {
    if (ssl_array != NULL) {
      SSL_free(ssl_array[i]);
      ssl_array[i] = NULL;
    }
    BIO_free(bio_array[i]);
    bio_array[i] = NULL;
  }
  if (ssl_array != NULL) {
    free(ssl_array);
    ssl_array = NULL;
  }
  free(bio_array);
  bio_array = NULL;
}

struct session_full_return add_to_session_sync(int related_fd) {
  assert(session_array != NULL);
  assert(bio_array != NULL);
  assert(related_fd > 0);

  struct session_full_return result = {NULL, NULL, NULL};

  pthread_mutex_lock(&session_mutex);
  // Ring buffer... could also used modular arithmetic
  int next = session_count < max_size ? session_count : session_last_in_index;
  assert(next < max_size);

  session_array[next].id = rand();
  session_array[next].related_fd = related_fd;
  session_array[next].thread_id = 0;
  int ret = BIO_reset(bio_array[next]);
  if (ret == -1) {
    printf("BIO_reset failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
    assert(ret != -1);
  }
  BIO_set_fd(bio_array[next], related_fd, BIO_NOCLOSE);

  if (session_count < max_size) {
    session_count++;
  } else {
    session_last_in_index = (session_last_in_index + 1) % max_size;
  }

  result.session = &session_array[next];
  result.bio = bio_array[next];
  if (ssl_array != NULL) {
    result.ssl = ssl_array[next];
  }
  pthread_mutex_unlock(&session_mutex);

  return result;
}

struct session_full_return get_session_sync(int related_fd) {
  assert(session_array != NULL);
  assert(bio_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  pthread_mutex_lock(&session_mutex);
  struct session_full_return result = {NULL, NULL, NULL};
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].related_fd == related_fd) {
      result.session = &session_array[i];
      result.bio = bio_array[i];
      if (ssl_array != NULL) {
        result.ssl = ssl_array[i];
      }
      break;
    }
  }
  pthread_mutex_unlock(&session_mutex);
  assert(result.session != NULL);
  return result;
}

// Get the session for a given thread id
struct session_full_return get_session_for_thread() {
  struct session_full_return result = {NULL, NULL, NULL};
  if (session_array == NULL) {
    return result;
  }
  pthread_mutex_lock(&session_mutex);
  unsigned long thread_id = (unsigned long)pthread_self();
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].thread_id == thread_id) {
      result.session = &session_array[i];
      result.bio = bio_array[i];
      if (ssl_array != NULL) {
        result.ssl = ssl_array[i];
      }
      break;
    }
  }
  pthread_mutex_unlock(&session_mutex);
  return result;
  assert(result.session != NULL);
}

// After a thread is done with a session, set the thread_id to 0 (unassign it)
void remove_thread_from_session() {
  assert(session_array != NULL);
  pthread_mutex_lock(&session_mutex);
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].thread_id == (unsigned long)pthread_self()) {
      session_array[i].thread_id = 0;
      break;
    }
  }
  pthread_mutex_unlock(&session_mutex);
}

// Assign a thread to a session.
// Caused by having a worker queue, and we need to know which session a worker
// is handling
struct session_full_return add_thread_to_session(int related_fd) {
  struct session_full_return result = {NULL, NULL, NULL};
  assert(session_array != NULL);
  pthread_mutex_lock(&session_mutex);
  int found = 0;
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].related_fd == related_fd) {
      found = 1;
      session_array[i].thread_id = (unsigned long)pthread_self();
      result.session = &session_array[i];
      result.bio = bio_array[i];
      if (ssl_array != NULL) {
        result.ssl = ssl_array[i];
      }
    }
  }
  pthread_mutex_unlock(&session_mutex);
  assert(found == 1);
  return result;
}

void del_from_session_sync(int related_fd) {
  assert(session_array != NULL);
  assert(bio_array != NULL);
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
    SSL *ssl = (ssl_array != NULL) ? ssl_array[i] : NULL;
    assert(ssl == NULL && ssl_array == NULL);
    BIO *bio = bio_array[i];
    int ret = BIO_reset(bio);
    if (ret == -1) {
      printf("BIO_reset failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
      assert(ret != -1);
    }

    for (; i < session_count - 1; i++) {
      session_array[i].related_fd = session_array[i + 1].related_fd;
      session_array[i].id = session_array[i + 1].id;
      session_array[i].thread_id = session_array[i + 1].thread_id;
      if (ssl_array != NULL)
        ssl_array[i] = ssl_array[i + 1];
      bio_array[i] = bio_array[i + 1];
      session_array[i + 1].thread_id = 0;
      session_array[i + 1].related_fd = 0;
      session_array[i + 1].id = 0;
      if (ssl_array != NULL)
        ssl_array[i + 1] = ssl;
      bio_array[i + 1] = bio;
    }

    if (session_count > 0)
      session_count--;

    if (i < session_last_in_index)
      session_last_in_index--;
  }

  pthread_mutex_unlock(&session_mutex);
}
