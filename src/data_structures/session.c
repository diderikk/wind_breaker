#include "session.h"
#include "../properties.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "buffer.h"
#include "marked_fds.h"
#include <openssl/err.h>
#include <stdbool.h>

static session_node_t *session_node_array = NULL;
// Eight states (8 bits)
// static char **status_array = NULL;

// Each state is represented by a linked list
static session_node_t *ll_initial = NULL;
static session_node_t *ll_has_been_read = NULL;
static session_node_t *ll_parsed = NULL;
static session_node_t *ll_data_fetched = NULL;
static session_node_t *ll_ready_to_send = NULL;

static int max_size = 0;
static unsigned int session_count = 0;
static unsigned int session_last_index = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_read_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t parsed_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t data_fetched_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t response_generated_cond = PTHREAD_COND_INITIALIZER;

static inline int init_session(int related_fd);
static inline void deinit_session(int i);
static inline int init_ssl_session(int index);
static inline void reset_session_at_index(int index);
static inline void reset_request_at_index(int index);
static void append_linked_list(session_node_t **ll, session_node_t *node);
static session_node_t *pop_linked_list(session_node_t **ll);

int init_session_cache(SSL_CTX *ctx) {
  SSL_library_init();
  assert(session_node_array == NULL);

  max_size = get_session_max_size();
  session_node_array = calloc(get_session_max_size(), sizeof(session_node_t));
  for (int i = 0; i < get_session_max_size(); i++) {
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
  session_count = 0;
  session_last_index = 0;

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
  ll_initial = NULL;
  ll_has_been_read = NULL;
  ll_parsed = NULL;
  ll_data_fetched = NULL;
  ll_ready_to_send = NULL;
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

void push_session(int related_fd, WORK_STATUS status) {
  pthread_mutex_lock(&session_mutex);

  assert(session_node_array != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  int found = 0;
  bool is_new_session =
      status == WORK_STATUS_INITIAL_SSL || status == WORK_STATUS_INITIAL;
  for (int i = 0; i < session_count; i++) {
    if (is_new_session)
      break;

    if (session_node_array[i].session.meta.related_fd == related_fd) {
      found = 1;
      session_node_t* node = &session_node_array[i];

      switch (status) {
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
        if(node->session.failed_send_attempts >= 3) {
          deinit_session(i);
        } else {
          append_linked_list(&ll_ready_to_send, node);
          pthread_cond_broadcast(&response_generated_cond);
        }
        break;
      case WORK_STATUS_SENT:
        node->session.failed_send_attempts = 0;
        append_linked_list(&ll_initial, node);
        reset_request_at_index(i);
        break;
      case WORK_STATUS_REJECTED:
        deinit_session(i);
        break;

      default:
        assert(0);
        break;
      }
    }
  }

  if (found != 1 && is_new_session) {
    int index = init_session(related_fd);
    if (status == WORK_STATUS_INITIAL_SSL) {
      switch (init_ssl_session(index)) {
      case 1:
        session_node_array[index].session.request.is_ssl = 1;
        break;
      case -1:
        deinit_session(index);
        break;
      default:
        assert(0);
        break;
      }
    }
  }

  pthread_mutex_unlock(&session_mutex);
}

static session_node_t* pop_next(WORK_STATUS status) {
  switch (status) {
    case WORK_STATUS_REQUEST_READ:
      return pop_linked_list(&ll_ready_to_send); 
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
  int index = -1;
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

  pthread_mutex_unlock(&session_mutex);
  return &result->session;
}

session_t *pop_session_for_read(int related_fd) {
  pthread_mutex_lock(&session_mutex);

  assert(session_node_array != NULL);
  assert(ll_initial != NULL);
  assert(related_fd > 0);
  assert(related_fd < 16384);

  session_t *result = NULL;
  int process = 0;
  while(ll_initial != NULL) {
    if(ll_initial->session.meta.related_fd == related_fd) {
      result = 
    }
  }
  

  pthread_mutex_unlock(&session_mutex);
  return result;
}

static inline int init_session(int related_fd) {
  int next = session_count < max_size ? session_count : session_last_index;
  assert(next < max_size);

  reset_session_at_index(next);

  session_t *session = &session_node_array[next].session;

  session->meta.id = rand();
  session->meta.related_fd = related_fd;
  BIO_set_fd(session->bio, related_fd, BIO_NOCLOSE);
  BIO_set_nbio(session->bio, 1);

  if (session_count < max_size) {
    session_count++;
  } else {
    session_last_index = (session_last_index + 1) % max_size;
  }

  return next;
}

static inline void deinit_session(int i) {
  session_node_t node = session_node_array[i];
  mark(node.session.meta.related_fd);

  reset_session_at_index(i);

  for (; i < session_count - 1; i++) {
    // Shift all existing sessions to the left
    session_node_array[i] = session_node_array[i + 1];

    // Replace the left shifted session with the to-be-deleted session
    session_node_array[i + 1] = node;
  }

  if (session_count > 0)
    session_count--;

  if (i < session_last_index)
    session_last_index--;
}

static inline int init_ssl_session(int index) {
  assert(session_node_array != NULL);

  SSL *ssl = session_node_array[index].session.ssl;
  BIO *bio = session_node_array[index].session.bio;

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

static inline void reset_session_at_index(int index) {
  session_t *session = &session_node_array[index].session;

  session->failed_send_attempts = 0;

  // Meta
  memset(&session->meta, 0, sizeof(meta_t));

  reset_request_at_index(index);

  // Response
  memset(&session->response, 0, sizeof(http_response_t));

  // BIO
  int ret = BIO_reset(session->bio);
  if (ret == -1) {
    printf("BIO_reset failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
    assert(ret != -1);
  }
  BIO_set_fd(session->bio, -1, BIO_NOCLOSE);
  BIO_set_nbio(session->bio, 1);

  if (session->ssl != NULL) {
    SSL *ssl = session->ssl;
    if (ssl != NULL) {
      SSL_set_bio(ssl, session->bio, session->bio);
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
  session_t *session = &session_node_array[index].session;
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

