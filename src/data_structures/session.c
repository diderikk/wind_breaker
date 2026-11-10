#include "session.h"
#include "../properties.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include "buffer.h"
#include "marked_fds.h"
#include <openssl/err.h>
#include <stdbool.h>
#include <stdlib.h>

// TODO node to fd table

static session_node_t *session_node_array = NULL;
// Eight states (8 bits)
// static char **status_array = NULL;

// Each state is represented by a linked list
static session_node_t *ll_ready_to_read = NULL;
static session_node_t *ll_has_been_read = NULL;
static session_node_t *ll_parsed = NULL;
static session_node_t *ll_data_fetched = NULL;
static session_node_t *ll_ready_to_send = NULL;
static session_node_t *ll_free = NULL;

static int max_size = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_read_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t parsed_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t data_fetched_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t response_generated_cond = PTHREAD_COND_INITIALIZER;

static inline session_node_t *init_session(int related_fd);
static inline void deinit_session(session_node_t *node);
static inline int init_ssl_session(session_node_t *node);
static inline void reset_session(session_node_t *node);
static inline void reset_request(session_t *session);
static void append_linked_list(session_node_t **ll, session_node_t *node);
static session_node_t *pop_linked_list(session_node_t **ll);
static session_node_t *splice_by_fd_linked_list(session_node_t **ll, int fd);

int init_session_cache(SSL_CTX *ctx) {
  SSL_library_init();
  assert(session_node_array == NULL);

  // destroy_session_cache() destroys these. glibc lets a destroyed static
  // mutex/cond be reused, but on macOS it becomes invalid, so re-initialize
  // them every time the cache is (re)created.
  assert(pthread_mutex_init(&session_mutex, NULL) == 0);
  assert(pthread_cond_init(&request_read_cond, NULL) == 0);
  assert(pthread_cond_init(&parsed_cond, NULL) == 0);
  assert(pthread_cond_init(&data_fetched_cond, NULL) == 0);
  assert(pthread_cond_init(&response_generated_cond, NULL) == 0);

  max_size = get_session_max_size();
  session_node_array = calloc(max_size, sizeof(session_node_t));
  assert(session_node_array != NULL);
  for (int i = 0; i < max_size; i++) {
    session_node_t *node = &session_node_array[i];
    node->next = NULL;
    session_t *session = &node->session;
    session->failed_send_attempts = 0;
    session->buffer = init_buffer(0);
    assert(session->buffer != NULL);
    session->bio = BIO_new(BIO_s_socket());
    assert(session->bio != NULL);
    if (ctx != NULL) {
      session->ssl = SSL_new(ctx);
      assert(session->ssl != NULL);
      assert(SSL_is_server(session->ssl));
    }
    // meta_t, http_request_t, http_response_t should be zeroed
    append_linked_list(&ll_free, node);
  }

  return 0;
}

void destroy_session_cache() {
  assert(session_node_array != NULL);

  assert(pthread_mutex_destroy(&session_mutex) == 0);
  assert(pthread_cond_destroy(&request_read_cond) == 0);
  assert(pthread_cond_destroy(&parsed_cond) == 0);
  assert(pthread_cond_destroy(&data_fetched_cond) == 0);
  assert(pthread_cond_destroy(&response_generated_cond) == 0);

  // Session array
  for (int i = 0; i < get_session_max_size(); i++) {
    session_node_t *node = &session_node_array[i];
    session_t *session = &node->session;
    if (session->buffer != NULL) {
      deinit_buffer(session->buffer);
      session->buffer = NULL;
    }
    if (session->ssl != NULL) {

      if (SSL_get_rbio(session->ssl) == NULL &&
          SSL_get_wbio(session->ssl) == NULL) {
        BIO_free(session->bio);
      }
      SSL_free(session->ssl);
      session->ssl = NULL;
    } else {
      BIO_free(session->bio);
    }
    session->bio = NULL;
  }
  free(session_node_array);
  session_node_array = NULL;

  // Status array
  ll_ready_to_read = NULL;
  ll_has_been_read = NULL;
  ll_parsed = NULL;
  ll_data_fetched = NULL;
  ll_ready_to_send = NULL;
  ll_free = NULL;
}

void broadcast_session() {
  assert(session_node_array != NULL);

  pthread_mutex_lock(&session_mutex);
  pthread_cond_broadcast(&request_read_cond);
  pthread_cond_broadcast(&parsed_cond);
  pthread_cond_broadcast(&data_fetched_cond);
  pthread_cond_broadcast(&response_generated_cond);
  pthread_mutex_unlock(&session_mutex);
}

void request_close_session(int related_fd) {
  pthread_mutex_lock(&session_mutex);
  assert(session_node_array != NULL);
  assert(related_fd > 0);

  // ll_ready_to_read is the only state this is not fetched by a handler (idle
  // state)
  session_node_t *node =
      splice_by_fd_linked_list(&ll_ready_to_read, related_fd);
  if (node != NULL) {
    deinit_session(node);
  } else {
    for (int i = 0; i < max_size; i++) {
      if (session_node_array[i].session.meta.related_fd == related_fd) {
        session_node_array[i].session.meta.closing = 1;
        break;
      }
    }
  }

  pthread_mutex_unlock(&session_mutex);
}

int push_session(int related_fd, WORK_STATUS status) {
  pthread_mutex_lock(&session_mutex);

  assert(session_node_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  int result = 0;
  int found = 0;
  bool is_new_session =
      status == WORK_STATUS_INITIAL_SSL || status == WORK_STATUS_INITIAL;
  for (int i = 0; i < max_size; i++) {
    if (is_new_session)
      break;

    if (session_node_array[i].session.meta.related_fd == related_fd) {
      found = 1;
      session_node_t *node = &session_node_array[i];
      if (node->session.meta.closing == 1)
        status = WORK_STATUS_REJECTED;

      switch (status) {
      case WORK_STATUS_RETRY:
        append_linked_list(&ll_has_been_read, node);
        break;
      case WORK_STATUS_REQUEST_READ:
        // TODO: Broadcast?
        // TODO: Remove old sessions
        append_linked_list(&ll_has_been_read, node);
        pthread_cond_broadcast(&request_read_cond);
        break;
      case WORK_STATUS_PARSED:
        append_linked_list(&ll_parsed, node);
        pthread_cond_broadcast(&parsed_cond);
        break;
      case WORK_STATUS_DATA_FETCHED:
        append_linked_list(&ll_data_fetched, node);
        pthread_cond_broadcast(&data_fetched_cond);
        break;
      case WORK_STATUS_READY_TO_SEND:
        append_linked_list(&ll_ready_to_send, node);
        pthread_cond_broadcast(&response_generated_cond);
        break;
      case WORK_STATUS_SEND_FAILED:
        node->session.failed_send_attempts += 1;
        if (node->session.failed_send_attempts >= 3) {
          deinit_session(node);
        } else {
          append_linked_list(&ll_ready_to_send, node);
          pthread_cond_broadcast(&response_generated_cond);
        }
        break;
      case WORK_STATUS_SENT:
        node->session.failed_send_attempts = 0;
        reset_request(&node->session);
        append_linked_list(&ll_ready_to_read, node);
        break;
      case WORK_STATUS_REJECTED:
        deinit_session(node);
        break;

      default:
        assert(0);
        break;
      }
    }
  }

  if (is_new_session) {
    bool is_ready = true;
    session_node_t *node = init_session(related_fd);
    if (node == NULL) {
      is_ready = false;
      mark(related_fd);
    } else {
      if (status == WORK_STATUS_INITIAL_SSL) {
        switch (init_ssl_session(node)) {
        case 1:
          node->session.request.is_ssl = 1;
          break;
        case -1:
          deinit_session(node);
          is_ready = false;
          break;
        default:
          assert(0);
          break;
        }
      }
    }
    if (is_ready)
      append_linked_list(&ll_ready_to_read, node);
    else
      result = -1;
  }

  pthread_mutex_unlock(&session_mutex);
  return result;
}

static session_node_t *pop_next(WORK_STATUS status) {
  switch (status) {
  case WORK_STATUS_REQUEST_READ:
    return pop_linked_list(&ll_has_been_read);
  case WORK_STATUS_PARSED:
    return pop_linked_list(&ll_parsed);
  case WORK_STATUS_DATA_FETCHED:
    return pop_linked_list(&ll_data_fetched);
  case WORK_STATUS_READY_TO_SEND:
    return pop_linked_list(&ll_ready_to_send);
  default:
    assert(0);
    return NULL;
    break;
  }
}

session_t *pop_session(WORK_STATUS status) {
  pthread_mutex_lock(&session_mutex);

  assert(session_node_array != NULL);
  session_node_t *result = NULL;
  while ((result = pop_next(status)) == NULL && !is_shutdown_requested()) {
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

  if (is_shutdown_requested()) {
    log_info("I am closing now!");
  }

  pthread_mutex_unlock(&session_mutex);
  return (result == NULL) ? NULL : &result->session;
}

session_t *pop_session_for_read(int related_fd) {
  pthread_mutex_lock(&session_mutex);

  assert(session_node_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  session_t *result = NULL;
  session_node_t *node =
      splice_by_fd_linked_list(&ll_ready_to_read, related_fd);

  if (node != NULL) {
    result = &node->session;
  }

  pthread_mutex_unlock(&session_mutex);
  return result;
}

static inline session_node_t *init_session(int related_fd) {
  session_node_t *node = pop_linked_list(&ll_free);

  if (node != NULL) {
    reset_session(node);

    session_t *session = &node->session;

    session->meta.id = rand();
    session->meta.related_fd = related_fd;
    BIO_set_fd(session->bio, related_fd, BIO_NOCLOSE);
    BIO_set_nbio(session->bio, 1);
  }

  return node;
}

static inline void deinit_session(session_node_t *node) {
  mark(node->session.meta.related_fd);

  reset_session(node);

  append_linked_list(&ll_free, node);
}

static inline int init_ssl_session(session_node_t *node) {
  assert(session_node_array != NULL);

  SSL *ssl = node->session.ssl;
  BIO *bio = node->session.bio;

  SSL_set_bio(ssl, bio, bio);

  int ret = SSL_accept(ssl);
  if (ret <= 0 || ret == 2) {
    int err = SSL_get_error(ssl, ret);
    // According to the SSL_accept, non-blocking socket must be handled
    if (err == SSL_ERROR_WANT_READ) {
      log_error("SSL_ERROR_WANT_READ");
      return 1;
    } else if (ERR_GET_REASON(ERR_peek_error()) == SSL_R_HTTP_REQUEST) {
      log_error("SSL_R_HTTP_REQUEST\n");
      return -1;
    } else {
      log_error("SSL_accept failed, %s\n",
                ERR_error_string(ERR_get_error(), NULL));
      return -1;
    }
  }
  return ret;
}

static inline void reset_session(session_node_t *node) {
  session_t *session = &node->session;

  session->failed_send_attempts = 0;

  // Meta
  memset(&session->meta, 0, sizeof(meta_t));

  // Request
  reset_request(session);
  session->request.is_ssl = 0;

  // Response
  memset(&session->response, 0, sizeof(http_response_t));

  // BIO
  int ret = BIO_reset(session->bio);
  if (ret == -1) {
    log_error("BIO_reset failed: %s\n",
              ERR_error_string(ERR_get_error(), NULL));
    assert(ret != -1);
  }
  BIO_set_fd(session->bio, -1, BIO_NOCLOSE);
  BIO_set_nbio(session->bio, 1);

  // SSL
  if (session->ssl != NULL) {
    SSL *ssl = session->ssl;
    if (ssl != NULL) {
      SSL_set_bio(ssl, session->bio, session->bio);
      ret = SSL_clear(ssl);
      if (ret != 1) {
        log_error("SSL_clear failed: %s\n",
                  ERR_error_string(ERR_get_error(), NULL));
        assert(ret == 1);
      }
    }
  }
}

static inline void reset_request(session_t *session) {
  char is_ssl = session->request.is_ssl;

  // Buffer
  buffer *buffer = session->buffer;
  if (buffer->data != NULL) {
    free(buffer->data);
    buffer->data = NULL;
  }

  buffer->capacity = 0;
  buffer->count = 0;

  // Request
  memset(&session->request, 0, sizeof(http_request_t));
  session->request.is_ssl = is_ssl;
}

static void append_linked_list(session_node_t **ll, session_node_t *node) {
  if (*ll == NULL) {
    *ll = node;
    node->tail = node;
  } else {
    session_node_t *head = *ll;
    head->tail->next = node;
    head->tail = node;
  }
}

static session_node_t *pop_linked_list(session_node_t **ll) {
  if (*ll == NULL)
    return NULL;

  session_node_t *head = *ll;
  session_node_t *new_head = head->next;

  if (head->next != NULL) {
    new_head->tail = head->tail;
  }
  *ll = new_head;

  head->next = NULL;
  head->tail = NULL;

  return head;
}

static session_node_t *splice_by_fd_linked_list(session_node_t **ll, int fd) {
  session_node_t *curr = *ll;
  session_node_t *prev = NULL;

  while (curr != NULL) {
    if (curr->session.meta.related_fd == fd) {
      if (prev == NULL) {
        return pop_linked_list(ll);
      } else {
        prev->next = curr->next;
        // We are splicing the tail
        if (curr->next == NULL)
          (*ll)->tail = prev;
        curr->next = NULL;
      }

      break;
    }

    prev = curr;
    curr = curr->next;
  }

  return curr;
}
