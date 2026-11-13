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

#define TABLE_MAX_LOAD 0.75

typedef enum { EMPTY, LIVE, TOMBSTONE } ENTRY_STATUS;

// Table guide from crafting interpreters
typedef struct {
  ENTRY_STATUS status;
  int fd;
  session_node_t *node;
} entry;

typedef struct {
  unsigned int live; // LIVE
  unsigned int used; // LIVE / TOMBSTONE
  unsigned int capacity;
  entry *entries;
} map;

static SSL_CTX *ssl_ctx = NULL;
static map *session_map = NULL;

// identity hashing (may be a performance issue later)
static inline unsigned int hash_fd(int fd, unsigned int capacity) {
  return fd & (capacity - 1);
}

// Each state is represented by a linked list
static session_node_t *ll_ready_to_read = NULL;
static session_node_t *ll_has_been_read = NULL;
static session_node_t *ll_parsed = NULL;
static session_node_t *ll_data_fetched = NULL;
static session_node_t *ll_ready_to_send = NULL;

static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_read_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t parsed_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t data_fetched_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t response_generated_cond = PTHREAD_COND_INITIALIZER;

static void destroy_session_node(session_node_t *node);
static inline session_node_t *init_session(int related_fd);
static inline void deinit_session(session_node_t *node);
static inline int init_ssl_session(session_node_t *node);
static inline void reset_request(session_t *session);
static void append_linked_list(session_node_t **ll, session_node_t *node);
static session_node_t *pop_linked_list(session_node_t **ll);
static session_node_t *splice_by_fd_linked_list(session_node_t **ll, int fd);
static void session_map_set(int fd, session_node_t *node);
static session_node_t *session_map_get(int fd);
static void session_map_delete(int fd);

int init_session_cache(SSL_CTX *ctx) {
  SSL_library_init();
  assert(session_map == NULL);

  // destroy_session_cache() destroys these. glibc lets a destroyed static
  // mutex/cond be reused, but on macOS it becomes invalid, so re-initialize
  // them every time the cache is (re)created.
  assert(pthread_mutex_init(&session_mutex, NULL) == 0);
  assert(pthread_cond_init(&request_read_cond, NULL) == 0);
  assert(pthread_cond_init(&parsed_cond, NULL) == 0);
  assert(pthread_cond_init(&data_fetched_cond, NULL) == 0);
  assert(pthread_cond_init(&response_generated_cond, NULL) == 0);

  // Store SSL Context for init_session
  ssl_ctx = ctx;

  session_map = calloc(1, sizeof(map));
  assert(session_map != NULL);

  return 0;
}

static void destroy_session_node(session_node_t *node) {
  session_t *session = &node->session;
  if (session->buffer != NULL) {
    deinit_buffer(session->buffer);
    session->buffer = NULL;
  }
  if (session->ssl != NULL) {
    SSL_free(session->ssl);
    session->ssl = NULL;
  } else {
    BIO_free(session->bio);
  }
  session->bio = NULL;
  free(node);
}

void destroy_session_cache() {
  assert(session_map != NULL);

  assert(pthread_mutex_destroy(&session_mutex) == 0);
  assert(pthread_cond_destroy(&request_read_cond) == 0);
  assert(pthread_cond_destroy(&parsed_cond) == 0);
  assert(pthread_cond_destroy(&data_fetched_cond) == 0);
  assert(pthread_cond_destroy(&response_generated_cond) == 0);

  // Session array
  for (int i = 0; i < session_map->capacity; i++) {
    entry *map_entry = &session_map->entries[i];
    if (map_entry->status != LIVE)
      continue;
    destroy_session_node(map_entry->node);
  }
  free(session_map->entries);
  free(session_map);
  session_map = NULL;

  // Status array
  ll_ready_to_read = NULL;
  ll_has_been_read = NULL;
  ll_parsed = NULL;
  ll_data_fetched = NULL;
  ll_ready_to_send = NULL;
}

void broadcast_session() {
  assert(session_map != NULL);

  pthread_mutex_lock(&session_mutex);
  pthread_cond_broadcast(&request_read_cond);
  pthread_cond_broadcast(&parsed_cond);
  pthread_cond_broadcast(&data_fetched_cond);
  pthread_cond_broadcast(&response_generated_cond);
  pthread_mutex_unlock(&session_mutex);
}

void request_close_session(int related_fd) {
  pthread_mutex_lock(&session_mutex);
  assert(session_map != NULL);
  assert(related_fd > 0);

  // ll_ready_to_read is the only state this is not fetched by a handler (idle
  // state)
  session_node_t *node =
      splice_by_fd_linked_list(&ll_ready_to_read, related_fd);
  if (node != NULL) {
    deinit_session(node);
  } else {
    node = session_map_get(related_fd);
    if (node != NULL)
      node->session.meta.closing = 1;
  }

  pthread_mutex_unlock(&session_mutex);
}

int push_session(int related_fd, WORK_STATUS status) {
  pthread_mutex_lock(&session_mutex);

  assert(session_map != NULL);
  assert(related_fd > 0);
  assert(related_fd < MAX_FD);

  int result = 0;
  bool is_new_session =
      status == WORK_STATUS_INITIAL_SSL || status == WORK_STATUS_INITIAL;
  session_node_t *node = session_map_get(related_fd);
  if (!is_new_session && node == NULL)
    log_warn("Session for file description: %d not found", related_fd);

  if (!is_new_session && node != NULL) {
    if (node->session.meta.closing == 1)
      status = WORK_STATUS_REJECTED;

    switch (status) {
    case WORK_STATUS_RETRY:
      append_linked_list(&ll_ready_to_read, node);
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

  assert(session_map != NULL);
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

  assert(session_map != NULL);
  assert(related_fd > 0);
  assert(related_fd < MAX_FD);

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
  if (session_map->live > get_session_max_size())
    return NULL;
  session_node_t *node = calloc(1, sizeof(session_node_t));
  if (node == NULL)
    return NULL;

  session_t *session = &node->session;
  session->meta.id = rand();
  session->meta.related_fd = related_fd;
  session->failed_send_attempts = 0;
  session->buffer = init_buffer(0);

  if (session->buffer == NULL) {
    free(node);
    return NULL;
  }
  session->bio = BIO_new(BIO_s_socket());
  if (session->bio == NULL) {
    deinit_buffer(session->buffer);
    free(node);
    return NULL;
  }
  BIO_set_fd(session->bio, related_fd, BIO_NOCLOSE);

  session_map_set(related_fd, node);

  return node;
}

static inline void deinit_session(session_node_t *node) {
  mark(node->session.meta.related_fd);
  session_map_delete(node->session.meta.related_fd);

  destroy_session_node(node);
}

static inline int init_ssl_session(session_node_t *node) {
  session_t *session = &node->session;

  // SSL
  if (ssl_ctx != NULL) {
    session->ssl = SSL_new(ssl_ctx);
    if (session->ssl == NULL) {
      log_error("Failed to SSL_new\n");
      BIO_free(session->bio);
      deinit_buffer(session->buffer);
      free(node);
      return -1;
    }
    assert(SSL_is_server(session->ssl));
  }

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

static entry *find_entry(entry *entries, unsigned int capacity, int fd) {
  unsigned int hash = hash_fd(fd, capacity);
  entry *tombstone = NULL;
  // Because of TABLE_MAX_LOAD we will always have free spots
  while (true) {
    entry *e = &entries[hash];
    if (e->status == TOMBSTONE && tombstone == NULL) {
      tombstone = e;
    } else if (e->status == EMPTY) {
      return tombstone != NULL ? tombstone : e;
    } else if (e->fd == fd) {
      // We found the entry
      return e;
    }

    hash = (hash + 1) & (capacity - 1);
  }
}

static void session_map_adjust_capacity(unsigned int new_capacity) {
  entry *entries = calloc(new_capacity, sizeof(entry));
  assert(entries != NULL);

  session_map->live = 0;
  for (unsigned int i = 0; i < session_map->capacity; i++) {
    entry *existing = &session_map->entries[i];
    if (existing->status == EMPTY || existing->status == TOMBSTONE)
      continue;

    entry *dest = find_entry(entries, new_capacity, existing->fd);
    dest->status = LIVE;
    dest->fd = existing->fd;
    dest->node = existing->node;
    session_map->live++;
  }
  session_map->used = session_map->live;

  free(session_map->entries);

  session_map->entries = entries;
  session_map->capacity = new_capacity;
}

static void session_map_set(int fd, session_node_t *node) {
  if (session_map->used + 1 > session_map->capacity * TABLE_MAX_LOAD) {
    // To remove a lot tombstones (the find entry only stops a LIVE statuses)
    if (session_map->live + 1 <= session_map->capacity * TABLE_MAX_LOAD / 2) {
      session_map_adjust_capacity(session_map->capacity);
    } else {
      // Double the capacity
      unsigned int new_capacity =
          session_map->capacity == 0 ? 16 : session_map->capacity * 2;
      session_map_adjust_capacity(new_capacity);
    }
  }

  entry *e = find_entry(session_map->entries, session_map->capacity, fd);
  if (e->status == EMPTY) {
    session_map->used++;
  }
  if (e->status == EMPTY || e->status == TOMBSTONE) {
    session_map->live++;
  }

  e->status = LIVE;
  e->fd = fd;
  e->node = node;
}

static session_node_t *session_map_get(int fd) {
  if (session_map->live == 0)
    return NULL;

  entry *e = find_entry(session_map->entries, session_map->capacity, fd);
  if (e->status == EMPTY || e->status == TOMBSTONE)
    return NULL;

  return e->node;
}

static void session_map_delete(int fd) {
  if (session_map->live == 0)
    return;

  entry *e = find_entry(session_map->entries, session_map->capacity, fd);
  if (e->status == EMPTY || e->status == TOMBSTONE)
    return;

  e->status = TOMBSTONE;
  e->fd = -1;
  e->node = NULL;
  session_map->live--;
}
