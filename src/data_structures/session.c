#include "session.h"
#include "../shutdown/stop.h"
#include <openssl/err.h>

static struct session *session_array = NULL;
// Eight states (8 bits)
static char *status_array = NULL;
static char **in_buffer_array = NULL;
static unsigned int *in_buffer_size_array = NULL;
static char **out_buffer_array = NULL;
static http_request_t **request_array = NULL;
static http_response_t **response_array = NULL;
static SSL **ssl_array = NULL;
static BIO **bio_array = NULL;
static int max_size = 0;
static unsigned int session_count = 0;
static unsigned int session_last_in_index = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t processing_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t request_read_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t parsed_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t data_fetched_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t response_generated_cond = PTHREAD_COND_INITIALIZER;

static void assert_(const char *file, int line, const char *func,
                    const char *msg) {
  fprintf(stdout, "Assertion failed: %s:%d: %s: %s\n", file, line, func, msg);
  exit(EXIT_FAILURE);
}
#define assert(expr)                                                           \
  ((void)((expr) || (assert_(__FILE__, __LINE__, __func__, #expr), 0)))

int init_session_cache(int _max_size, SSL_CTX *ctx) {
  SSL_library_init();
  assert(_max_size > 0);
  assert(session_array == NULL);
  // Static array of sessions, should not be resized... Tiger style

  // Session array
  session_array = calloc(_max_size, sizeof(*session_array));
  assert(session_array != NULL);
  max_size = _max_size;

  // Status array
  status_array = calloc(max_size, sizeof(char));
  assert(status_array != NULL);

  // In Buffer array
  in_buffer_array = calloc(max_size, sizeof(char *) * REQUEST_RESPONSE_MAX_SIZE);
  assert(in_buffer_array != NULL);

  for (int i = 0; i < max_size; i++) {
    in_buffer_array[i] = calloc(REQUEST_RESPONSE_MAX_SIZE, sizeof(char));
    assert(in_buffer_array[i] != NULL);
  }

  // In Buffer size array
  in_buffer_size_array = calloc(max_size, sizeof(unsigned int));
  assert(in_buffer_size_array != NULL);

  // Out Buffer array
  out_buffer_array = calloc(max_size, sizeof(char *) * REQUEST_RESPONSE_MAX_SIZE);
  assert(out_buffer_array != NULL);

  for (int i = 0; i < max_size; i++) {
    out_buffer_array[i] = calloc(REQUEST_RESPONSE_MAX_SIZE, sizeof(char));
    assert(out_buffer_array[i] != NULL);
  }

  // Request array
  request_array = calloc(max_size, sizeof(http_request_t *));
  assert(request_array != NULL);

  for (int i = 0; i < max_size; i++) {
    request_array[i] = calloc(1, sizeof(http_request_t));
    assert(request_array[i] != NULL);
  }

  // Response array
  response_array = calloc(max_size, sizeof(http_response_t *));
  assert(response_array != NULL);

  for (int i = 0; i < max_size; i++) {
    response_array[i] = calloc(1, sizeof(http_response_t));
    assert(response_array[i] != NULL);
  }

  // BIO array
  bio_array = malloc(max_size * sizeof(BIO *));
  assert(bio_array != NULL);

  for (int i = 0; i < max_size; i++) {
    bio_array[i] = BIO_new(BIO_s_socket());
    BIO_set_nbio(bio_array[i], 1);
    assert(bio_array[i] != NULL);
  }

  // SSL array
  if (ctx != NULL) {
    ssl_array = malloc(max_size * sizeof(SSL *));
    assert(ssl_array != NULL);

    for (int i = 0; i < max_size; i++) {
      ssl_array[i] = SSL_new(ctx);
      assert(ssl_array[i] != NULL);
      assert(SSL_is_server(ssl_array[i]));
    }
  }

  return 0;
}

void broadcast_session() {
  assert(session_array != NULL);
  assert(status_array != NULL);
  assert(in_buffer_array != NULL);
  assert(in_buffer_size_array != NULL);
  assert(out_buffer_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);

  pthread_mutex_lock(&session_mutex);
  pthread_cond_broadcast(&request_read_cond);
  pthread_cond_broadcast(&processing_cond);
  pthread_cond_broadcast(&parsed_cond);
  pthread_cond_broadcast(&data_fetched_cond);
  pthread_cond_broadcast(&response_generated_cond);
  pthread_mutex_unlock(&session_mutex);
}

void destroy_session_cache() {
  assert(session_array != NULL);
  assert(status_array != NULL);
  assert(in_buffer_array != NULL);
  assert(in_buffer_size_array != NULL);
  assert(out_buffer_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);

  assert(pthread_mutex_destroy(&session_mutex) == 0);
  assert(pthread_cond_destroy(&request_read_cond) == 0);
  assert(pthread_cond_destroy(&processing_cond) == 0);
  assert(pthread_cond_destroy(&parsed_cond) == 0);
  assert(pthread_cond_destroy(&data_fetched_cond) == 0);
  assert(pthread_cond_destroy(&response_generated_cond) == 0);
  session_count = 0;
  session_last_in_index = 0;

  // Session array
  free(session_array);
  session_array = NULL;

  // Status array
  free(status_array);
  status_array = NULL;

  // In Buffer array
  for (int i = 0; i < max_size; i++) {
    if (in_buffer_array[i] != NULL) {
      free(in_buffer_array[i]);
      in_buffer_array[i] = NULL;
    }
  }
  free(in_buffer_array);
  in_buffer_array = NULL;

  // Buffer size array
  free(in_buffer_size_array);
  in_buffer_size_array = NULL;

  // Out Buffer array
  for (int i = 0; i < max_size; i++) {
    if (out_buffer_array[i] != NULL) {
      free(out_buffer_array[i]);
      out_buffer_array[i] = NULL;
    }
  }
  free(out_buffer_array);
  out_buffer_array = NULL;

  // Request array
  for (int i = 0; i < max_size; i++) {
    if (request_array[i] != NULL) {
      free(request_array[i]);
      request_array[i] = NULL;
    }
  }
  free(request_array);
  request_array = NULL;

  // Response array
  for (int i = 0; i < max_size; i++) {
    if (response_array[i] != NULL) {
      free(response_array[i]);
      response_array[i] = NULL;
    }
  }
  free(response_array);
  response_array = NULL;

  // BIO array and SSL array
  for (int i = 0; i < max_size; i++) {
    if (ssl_array != NULL) {
      if (SSL_get_rbio(ssl_array[i]) == NULL &&
          SSL_get_wbio(ssl_array[i]) == NULL) {
        BIO_free(bio_array[i]);
      }
      SSL_free(ssl_array[i]);
      ssl_array[i] = NULL;
    } else {
      BIO_free(bio_array[i]);
    }
    bio_array[i] = NULL;
  }
  if (ssl_array != NULL) {
    free(ssl_array);
    ssl_array = NULL;
  }
  free(bio_array);
  bio_array = NULL;
}

static inline void reset_session_at_index(int index) {
  // Session array
  session_array[index].id = 0;
  session_array[index].related_fd = 0;
  session_array[index].thread_id = 0;

  // Status array
  status_array[index] = 0;

  // Buffer array
  memset(in_buffer_array[index], 0, REQUEST_RESPONSE_MAX_SIZE);

  // Buffer size array
  in_buffer_size_array[index] = 0;

  // Out Buffer array
  memset(out_buffer_array[index], 0, REQUEST_RESPONSE_MAX_SIZE);

  // Request array
  memset(request_array[index], 0, sizeof(http_request_t));

  // Response array
  memset(response_array[index], 0, sizeof(http_response_t));

  // BIO array
  int ret = BIO_reset(bio_array[index]);
  if (ret == -1) {
    printf("BIO_reset failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
    assert(ret != -1);
  }
  BIO_set_fd(bio_array[index], -1, BIO_NOCLOSE);
}

// Get the session for a given thread id
int get_session_id_for_thread() {
  int session_id = -1;
  if (session_array != NULL) {
    pthread_mutex_lock(&session_mutex);
    unsigned long thread_id = (unsigned long)pthread_self();
    for (int i = 0; i < session_count; i++) {
      if (session_array[i].thread_id == thread_id) {
        session_id = session_array[i].id;
        break;
      }
    }
    pthread_mutex_unlock(&session_mutex);
  }
  return session_id;
}

static inline void add_to_session(int related_fd) {
  int next = session_count < max_size ? session_count : session_last_in_index;
  assert(next < max_size);

  reset_session_at_index(next);

  session_array[next].id = rand();
  session_array[next].related_fd = related_fd;
  session_array[next].thread_id = 0;
  status_array[next] = WORK_STATUS_INITIAL;
  BIO_set_fd(bio_array[next], related_fd, BIO_NOCLOSE);

  if (session_count < max_size) {
    session_count++;
  } else {
    session_last_in_index = (session_last_in_index + 1) % max_size;
  }
}

static inline void del_from_session(int i) {
  char *in_buffer = in_buffer_array[i];
  char *out_buffer = out_buffer_array[i];
  http_request_t *request = request_array[i];
  http_response_t *response = response_array[i];
  BIO *bio = bio_array[i];
  SSL *ssl = (ssl_array != NULL) ? ssl_array[i] : NULL;
  assert(ssl == NULL || ssl_array != NULL);

  reset_session_at_index(i);

  for (; i < session_count - 1; i++) {
    // Shift all existing sessions to the left
    session_array[i].related_fd = session_array[i + 1].related_fd;
    session_array[i].id = session_array[i + 1].id;
    session_array[i].thread_id = session_array[i + 1].thread_id;
    status_array[i] = status_array[i + 1];
    in_buffer_array[i] = in_buffer_array[i + 1];
    in_buffer_size_array[i] = in_buffer_size_array[i + 1];
    out_buffer_array[i] = out_buffer_array[i + 1];
    request_array[i] = request_array[i + 1];
    response_array[i] = response_array[i + 1];
    bio_array[i] = bio_array[i + 1];
    if (ssl_array != NULL)
      ssl_array[i] = ssl_array[i + 1];

    // Replace the left shifted session with the to-be-deleted session
    session_array[i + 1].thread_id = 0;
    session_array[i + 1].related_fd = 0;
    session_array[i + 1].id = 0;
    status_array[i + 1] = 0;
    in_buffer_array[i + 1] = in_buffer;
    in_buffer_size_array[i + 1] = 0;
    out_buffer_array[i + 1] = out_buffer;
    request_array[i + 1] = request;
    response_array[i + 1] = response;
    if (ssl_array != NULL)
      ssl_array[i + 1] = ssl;
    bio_array[i + 1] = bio;
  }

  if (session_count > 0)
    session_count--;

  if (i < session_last_in_index)
    session_last_in_index--;
}

void push_request(int related_fd, WORK_STATUS status) {
  assert(session_array != NULL);
  assert(status_array != NULL);
  assert(in_buffer_array != NULL);
  assert(in_buffer_size_array != NULL);
  assert(out_buffer_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);


  pthread_mutex_lock(&session_mutex);
  int found = 0;
  for (int i = 0; i < session_count; i++) {
    if (session_array[i].related_fd == related_fd) {
      found = 1;
      assert(status_array[i] & WORK_STATUS_PROCESSING);
      status_array[i] &= ~WORK_STATUS_PROCESSING;
      status_array[i] |= status;

      switch (status) {
      case WORK_STATUS_INITIAL:
        pthread_cond_broadcast(&processing_cond);
        break;
      case WORK_STATUS_REQUEST_READ:
        if (status_array[i] & WORK_STATUS_SENT) {
          status_array[i] = WORK_STATUS_INITIAL | WORK_STATUS_REQUEST_READ;
        }
        pthread_cond_broadcast(&request_read_cond);
        break;
      case WORK_STATUS_PARSED:
        pthread_cond_broadcast(&parsed_cond);
        break;
      case WORK_STATUS_DATA_FETCHED:
        pthread_cond_broadcast(&data_fetched_cond);
        break;
      case WORK_STATUS_RESPONSE_GENERATED:
        pthread_cond_broadcast(&response_generated_cond);
        break;
      case WORK_STATUS_PROCESSING:
        // Deadlocked - used for instance for failed send
        break;
      case WORK_STATUS_SENT:
        pthread_cond_broadcast(&processing_cond);
        break;
      case WORK_STATUS_REJECTED:
        del_from_session(i);
        break;

      default:
        assert(0);
        break;
      }
    }
  }

  if (found != 1 && status == WORK_STATUS_INITIAL) {
    add_to_session(related_fd);
  } 
  pthread_mutex_unlock(&session_mutex);
}

static inline int peek_next(WORK_STATUS status) {
  int index = -1;
  for (int i = 0; i < session_count; i++) {
    if (status_array[i] < status)
      continue;
    if (status_array[i] & WORK_STATUS_PROCESSING)
      continue;
    for (int bit = 7; bit >= 0; bit--) {
      // All significant bits should NOT be set
      if ((1 << bit) > status && (1 << bit) & status_array[i]) {
        break;
      }
      // This is the bit we are looking for
      else if ((1 << bit) == status && (1 << bit) & status_array[i]) {
        index = i;
        break;
      } // TODO: Check if remaining bits are set
    }
  }
  return index;
}

struct session_full_return pop_request(WORK_STATUS status) {
  assert(session_array != NULL);
  assert(in_buffer_array != NULL);
  assert(in_buffer_size_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);
  struct session_full_return result = {NULL, NULL, NULL, NULL,
                                       NULL, NULL, NULL, NULL};
  pthread_mutex_lock(&session_mutex);
  int index = -1;
  while ((index = peek_next(status)) == -1 && !stop()) {
    switch (status) {
    case WORK_STATUS_REQUEST_READ:
      pthread_cond_wait(&request_read_cond, &session_mutex);
      break;
    case WORK_STATUS_PARSED:
      pthread_cond_wait(&parsed_cond, &session_mutex);
      break;
    case WORK_STATUS_DATA_FETCHED:
      pthread_cond_wait(&data_fetched_cond, &session_mutex);
      break;
    case WORK_STATUS_RESPONSE_GENERATED:
      pthread_cond_wait(&response_generated_cond, &session_mutex);
      break;

    default:
      assert(0);
      break;
    }
  }
  if (index > -1) {
    status_array[index] |= WORK_STATUS_PROCESSING;

    result.session = &session_array[index];
    result.in_buffer = in_buffer_array[index];
    result.in_buffer_size = &in_buffer_size_array[index];
    result.out_buffer = out_buffer_array[index];
    result.request = request_array[index];
    result.response = response_array[index];
    result.bio = bio_array[index];
    if (ssl_array != NULL) {
      result.ssl = ssl_array[index];
    }
  }

  pthread_mutex_unlock(&session_mutex);
  return result;
}

struct session_full_return pop_request_by_fd(int related_fd) {
  assert(session_array != NULL);
  assert(status_array != NULL);
  assert(in_buffer_array != NULL);
  assert(in_buffer_size_array != NULL);
  assert(out_buffer_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  pthread_mutex_lock(&session_mutex);
  struct session_full_return result = {NULL, NULL, NULL, NULL,
                                       NULL, NULL, NULL, NULL};
  while (result.session == NULL && !stop()) {
    int in_process = 0;
    for (int i = 0; i < session_count; i++) {
      if (session_array[i].related_fd == related_fd) {
        assert(status_array[i] & WORK_STATUS_SENT ||
               status_array[i] == WORK_STATUS_INITIAL);

        if (status_array[i] & WORK_STATUS_PROCESSING) {
          in_process = 1;
          pthread_cond_wait(&processing_cond, &session_mutex);
          break;
        }
        status_array[i] |= WORK_STATUS_PROCESSING;
        result.session = &session_array[i];
        result.in_buffer = in_buffer_array[i];
        result.in_buffer_size = &in_buffer_size_array[i];
        result.out_buffer = out_buffer_array[i];
        result.request = request_array[i];
        result.response = response_array[i];
        result.bio = bio_array[i];
        if (ssl_array != NULL) {
          result.ssl = ssl_array[i];
        }
        break;
      }
    }
    assert(result.session != NULL || in_process == 1);
  }

  pthread_mutex_unlock(&session_mutex);
  return result;
}
