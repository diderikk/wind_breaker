#include "session.h"
#include "../properties.h"
#include "../shutdown/stop.h"
#include "marked_fds.h"
#include <execinfo.h>
#include <openssl/err.h>

static struct session **session_array = NULL;
// Eight states (8 bits)
static char **status_array = NULL;
static char **buffer_array = NULL;
static unsigned int **buffer_size_array = NULL;
static http_request_t **request_array = NULL;
static http_response_t **response_array = NULL;
static SSL **ssl_array = NULL;
static BIO **bio_array = NULL;
static int max_size = 0;
static unsigned int session_count = 0;
static unsigned int session_last_index = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_read_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t parsed_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t data_fetched_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t response_generated_cond = PTHREAD_COND_INITIALIZER;

static void assert_(const char *file, int line, const char *func,
                    const char *msg) {
  fprintf(stdout, "Assertion failed: %s:%d: %s: %s\n", file, line, func, msg);

  void *backtrace_buffer[BACKTRACE_SIZE];
  unsigned int backtrace_size = backtrace(backtrace_buffer, BACKTRACE_SIZE);
  char **backtrace_symbols_buffer =
      backtrace_symbols(backtrace_buffer, backtrace_size);
  if (backtrace_symbols_buffer != NULL && backtrace_size > 0) {
    printf("Backtrace:\n");
    for (unsigned int i = 0; i < backtrace_size; i++) {
      printf("  %d: %s\n", i, backtrace_symbols_buffer[i]);
    }
    free(backtrace_symbols_buffer);
  }

  raise(SIGABRT);
}
#define assert(expr)                                                           \
  ((void)((expr) || (assert_(__FILE__, __LINE__, __func__, #expr), 0)))

int init_session_cache(SSL_CTX *ctx) {
  SSL_library_init();
  assert(session_array == NULL);

  max_size = get_session_max_size();
  // Static array of sessions, should not be resized... Tiger style

  // Session array
  session_array = calloc(max_size, sizeof(struct session *));
  assert(session_array != NULL);
  for (int i = 0; i < max_size; i++) {
    session_array[i] = calloc(1, sizeof(struct session));
    assert(session_array[i] != NULL);
  }

  // Status array
  status_array = calloc(max_size, sizeof(char *));
  assert(status_array != NULL);
  for (int i = 0; i < max_size; i++) {
    status_array[i] = calloc(1, sizeof(char));
    assert(status_array[i] != NULL);
  }

  // Buffer array
  buffer_array = calloc(max_size, sizeof(char *));
  assert(buffer_array != NULL);

  for (int i = 0; i < max_size; i++) {
    buffer_array[i] = calloc(REQUEST_RESPONSE_MAX_SIZE, sizeof(char));
    assert(buffer_array[i] != NULL);
  }

  // Buffer size array
  buffer_size_array = calloc(max_size, sizeof(unsigned int *));
  assert(buffer_size_array != NULL);
  for (int i = 0; i < max_size; i++) {
    buffer_size_array[i] = calloc(1, sizeof(unsigned int));
    assert(buffer_size_array[i] != NULL);
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
  assert(buffer_array != NULL);
  assert(buffer_size_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);

  pthread_mutex_lock(&session_mutex);
  pthread_cond_broadcast(&request_read_cond);
  pthread_cond_broadcast(&parsed_cond);
  pthread_cond_broadcast(&data_fetched_cond);
  pthread_cond_broadcast(&response_generated_cond);
  pthread_mutex_unlock(&session_mutex);
}

void destroy_session_cache() {
  assert(session_array != NULL);
  assert(status_array != NULL);
  assert(buffer_array != NULL);
  assert(buffer_size_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);

  assert(pthread_mutex_destroy(&session_mutex) == 0);
  assert(pthread_cond_destroy(&request_read_cond) == 0);
  assert(pthread_cond_destroy(&parsed_cond) == 0);
  assert(pthread_cond_destroy(&data_fetched_cond) == 0);
  assert(pthread_cond_destroy(&response_generated_cond) == 0);
  session_count = 0;
  session_last_index = 0;

  // Session array
  for (int i = 0; i < max_size; i++) {
    if (session_array[i] != NULL) {
      free(session_array[i]);
      session_array[i] = NULL;
    }
  }
  free(session_array);
  session_array = NULL;

  // Status array
  for (int i = 0; i < max_size; i++) {
    if (status_array[i] != NULL) {
      free(status_array[i]);
      status_array[i] = NULL;
    }
  }
  free(status_array);
  status_array = NULL;

  // Buffer array
  for (int i = 0; i < max_size; i++) {
    if (buffer_array[i] != NULL) {
      free(buffer_array[i]);
      buffer_array[i] = NULL;
    }
  }
  free(buffer_array);
  buffer_array = NULL;

  // Buffer size array
  for (int i = 0; i < max_size; i++) {
    if (buffer_size_array[i] != NULL) {
      free(buffer_size_array[i]);
      buffer_size_array[i] = NULL;
    }
  }
  free(buffer_size_array);
  buffer_size_array = NULL;

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
  memset(session_array[index], 0, sizeof(struct session));

  // Status array
  *status_array[index] = 0;

  // Buffer array
  memset(buffer_array[index], 0, REQUEST_RESPONSE_MAX_SIZE);

  // Buffer size array
  *buffer_size_array[index] = 0;

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
  BIO_set_nbio(bio_array[index], 1);

  if (ssl_array != NULL) {
    SSL *ssl = ssl_array[index];
    if (ssl != NULL) {
      SSL_set_bio(ssl, bio_array[index], bio_array[index]);
      ret = SSL_clear(ssl);
      if (ret != 1) {
        printf("SSL_clear failed: %s\n",
               ERR_error_string(ERR_get_error(), NULL));
        assert(ret == 1);
      }
    }
  }
}

static inline void reset_request_at_index(int index) {
  char is_ssl = request_array[index]->is_ssl;
  // Buffer array
  memset(buffer_array[index], 0, REQUEST_RESPONSE_MAX_SIZE);

  // Buffer size array
  *buffer_size_array[index] = 0;

  // Request array
  memset(request_array[index], 0, sizeof(http_request_t));
  request_array[index]->is_ssl = is_ssl;
}

// Get the session for a given thread id
int get_session_id_for_thread() {
  int session_id = -1;
  if (session_array != NULL) {
    pthread_mutex_lock(&session_mutex);
    unsigned long thread_id = (unsigned long)pthread_self();
    for (int i = 0; i < session_count; i++) {
      if (session_array[i]->thread_id == thread_id) {
        session_id = session_array[i]->id;
        break;
      }
    }
    pthread_mutex_unlock(&session_mutex);
  }
  return session_id;
}

static inline int init_session_connection(int index) {
  assert(session_array != NULL);
  assert(bio_array != NULL);
  assert(ssl_array != NULL);

  SSL *ssl = ssl_array[index];
  BIO *bio = bio_array[index];

  SSL_set_bio(ssl, bio, bio);

  int ret = SSL_accept(ssl);
  if (ret <= 0 || ret == 2) {
    int err = SSL_get_error(ssl, ret);
    // According to the SSL_accept, non-blocking socket must be handled
    if (err == SSL_ERROR_WANT_READ) {
      printf("SSL_ERROR_WANT_READ");
      return 1;
    } else if (ERR_GET_REASON(ERR_peek_error()) == SSL_R_HTTP_REQUEST) {
      printf("SSL_R_HTTP_REQUEST\n");
      return -1;
    } else {
      printf("SSL_accept failed, %s\n",
             ERR_error_string(ERR_get_error(), NULL));
      return -1;
    }
  }
  return ret;
}

static inline int add_to_session(int related_fd) {
  int next = session_count < max_size ? session_count : session_last_index;
  assert(next < max_size);

  reset_session_at_index(next);

  session_array[next]->id = rand();
  session_array[next]->related_fd = related_fd;
  session_array[next]->thread_id = 0;
  *status_array[next] &= WORK_STATUS_INITIAL;
  BIO_set_fd(bio_array[next], related_fd, BIO_NOCLOSE);
  BIO_set_nbio(bio_array[next], 1);

  if (session_count < max_size) {
    session_count++;
  } else {
    session_last_index = (session_last_index + 1) % max_size;
  }

  return next;
}

static inline void del_from_session(int i) {
  struct session *session = session_array[i];
  char *status = status_array[i];
  char *buffer = buffer_array[i];
  unsigned int *buffer_size = buffer_size_array[i];
  http_request_t *request = request_array[i];
  http_response_t *response = response_array[i];
  BIO *bio = bio_array[i];
  SSL *ssl = (ssl_array != NULL) ? ssl_array[i] : NULL;
  assert(ssl == NULL || ssl_array != NULL);

  reset_session_at_index(i);

  for (; i < session_count - 1; i++) {
    // Shift all existing sessions to the left
    session_array[i] = session_array[i + 1];
    status_array[i] = status_array[i + 1];
    buffer_array[i] = buffer_array[i + 1];
    buffer_size_array[i] = buffer_size_array[i + 1];
    request_array[i] = request_array[i + 1];
    response_array[i] = response_array[i + 1];
    bio_array[i] = bio_array[i + 1];
    if (ssl_array != NULL)
      ssl_array[i] = ssl_array[i + 1];

    // Replace the left shifted session with the to-be-deleted session
    session_array[i + 1] = session;
    status_array[i + 1] = status;
    buffer_array[i + 1] = buffer;
    buffer_size_array[i + 1] = buffer_size;
    request_array[i + 1] = request;
    response_array[i + 1] = response;
    if (ssl_array != NULL)
      ssl_array[i + 1] = ssl;
    bio_array[i + 1] = bio;
  }

  if (session_count > 0)
    session_count--;

  if (i < session_last_index)
    session_last_index--;
}

void push_request(int related_fd, WORK_STATUS status) {
  assert(session_array != NULL);
  assert(status_array != NULL);
  assert(buffer_array != NULL);
  assert(buffer_size_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  pthread_mutex_lock(&session_mutex);
  int found = 0;
  for (int i = 0; i < session_count; i++) {
    if (session_array[i]->related_fd == related_fd) {
      found = 1;
      assert(*status_array[i] & WORK_STATUS_PROCESSING);
      *status_array[i] &= ~WORK_STATUS_PROCESSING;
      *status_array[i] |= status;

      switch (status) {
      case WORK_STATUS_INITIAL_SSL:
        *status_array[i] |= WORK_STATUS_INITIAL;
        break;
      case WORK_STATUS_INITIAL:
        break;
      case WORK_STATUS_REQUEST_READ:
        pthread_cond_broadcast(&request_read_cond);
        break;
      case WORK_STATUS_PARSED:
        pthread_cond_broadcast(&parsed_cond);
        break;
      case WORK_STATUS_DATA_FETCHED:
        pthread_cond_broadcast(&data_fetched_cond);
        break;
      case WORK_STATUS_READY_TO_SEND:
        pthread_cond_broadcast(&response_generated_cond);
        break;
      case WORK_STATUS_SEND_FAILED:
        *status_array[i] &= ~WORK_STATUS_READY_TO_SEND;
        break;
      case WORK_STATUS_SENT:
        *status_array[i] &=
            ~(WORK_STATUS_REQUEST_READ | WORK_STATUS_PARSED |
              WORK_STATUS_DATA_FETCHED | WORK_STATUS_READY_TO_SEND);
        reset_request_at_index(i);
        break;
      case WORK_STATUS_PROCESSING:
        // Deadlocked - used for instance for failed send
        break;
      case WORK_STATUS_REJECTED:
        del_from_session(i);
        mark(related_fd);
        break;

      default:
        assert(0);
        break;
      }
    }
  }

  if (found != 1 &&
      (status == WORK_STATUS_INITIAL || status == WORK_STATUS_INITIAL_SSL)) {
    int index = add_to_session(related_fd);
    if (status == WORK_STATUS_INITIAL_SSL) {
      switch (init_session_connection(index)) {
      case 1:
        request_array[index]->is_ssl = 1;
        break;
      case -1:
        del_from_session(index);
        mark(related_fd);
        break;
      default:
        assert(0);
        break;
      }
    }
  }
  pthread_mutex_unlock(&session_mutex);
}

static inline int peek_next(WORK_STATUS status) {
  int index = -1;
  for (int i = 0; i < session_count; i++) {
    if (*status_array[i] < status)
      continue;
    if (*status_array[i] & WORK_STATUS_PROCESSING)
      continue;
    if (*status_array[i] & WORK_STATUS_SEND_FAILED)
      continue;
    for (int bit = 4; bit >= 0; bit--) {
      // All significant bits should NOT be set
      if ((1 << bit) > status && (1 << bit) & *status_array[i]) {
        break;
      }
      // This is the bit we are looking for
      else if ((1 << bit) == status && status & *status_array[i]) {
        index = i;
        break;
      } // TODO: Check if remaining bits are set
    }
  }
  return index;
}

struct session_full_return pop_request(WORK_STATUS status) {
  assert(session_array != NULL);
  assert(buffer_array != NULL);
  assert(buffer_size_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);
  struct session_full_return result = {NULL, NULL, NULL, NULL,
                                       NULL, NULL, NULL};
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
    case WORK_STATUS_READY_TO_SEND:
      pthread_cond_wait(&response_generated_cond, &session_mutex);
      break;

    default:
      assert(0);
      break;
    }
  }
  if (index > -1) {
    *status_array[index] |= WORK_STATUS_PROCESSING;

    result.session = session_array[index];
    result.buffer = buffer_array[index];
    result.buffer_size = buffer_size_array[index];
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
  assert(buffer_array != NULL);
  assert(buffer_size_array != NULL);
  assert(request_array != NULL);
  assert(response_array != NULL);
  assert(bio_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  pthread_mutex_lock(&session_mutex);
  struct session_full_return result = {NULL, NULL, NULL, NULL,
                                       NULL, NULL, NULL};
  int process = 0;
  for (int i = 0; i < session_count; i++) {
    if (session_array[i]->related_fd == related_fd) {

      if (((*status_array[i] & WORK_STATUS_SEND_FAILED) == 0) &&
          (*status_array[i] & WORK_STATUS_REQUEST_READ ||
           *status_array[i] & WORK_STATUS_PARSED ||
           *status_array[i] & WORK_STATUS_DATA_FETCHED ||
           *status_array[i] & WORK_STATUS_PROCESSING ||
           *status_array[i] & WORK_STATUS_READY_TO_SEND)) {
        process = 1;
      } else {
        *status_array[i] |= WORK_STATUS_PROCESSING;
        result.session = session_array[i];
        result.buffer = buffer_array[i];
        result.buffer_size = buffer_size_array[i];
        result.request = request_array[i];
        result.response = response_array[i];
        result.bio = bio_array[i];
        if (ssl_array != NULL) {
          result.ssl = ssl_array[i];
        }
      }
      break;
    }
  }

  pthread_mutex_unlock(&session_mutex);
  return result;
}
