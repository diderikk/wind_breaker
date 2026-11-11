#include "../../../src/data_structures/marked_fds.h"
#include "../../../src/data_structures/session.h"
#include "../../../src/properties.h"
#include "../../../src/static.h"
#include "../../../src/utils/assert2.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SESSION_TEST_SIZE 5

static int session_test_count = 0;
static SSL_CTX *session_test_ctx = NULL;
static int session_start_case(void *(*func)(void *), const char *name);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void st_setup(int size) {
  set_session_max_size(size);
  init_marked_fds();
  init_session_cache(NULL);
}

static void st_setup_ssl(int size) {
  set_session_max_size(size);
  init_marked_fds();
  session_test_ctx = SSL_CTX_new(TLS_server_method());
  assert(session_test_ctx != NULL);
  init_session_cache(session_test_ctx);
}

static void st_teardown() {
  destroy_session_cache();
  destroy_marked_fds();
  if (session_test_ctx != NULL) {
    SSL_CTX_free(session_test_ctx);
    session_test_ctx = NULL;
  }
}

// Connected pair of sockets. The server side (sv[0]) is non-blocking like the
// sockets the listener accepts, the BIO does not make it non-blocking itself.
static void st_socketpair(int sv[2]) {
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  int flags = fcntl(sv[0], F_GETFL, 0);
  assert(flags != -1);
  assert(fcntl(sv[0], F_SETFL, flags | O_NONBLOCK) != -1);
}

// New plain connection that the "listener" has popped to read from
static session_t *st_open(int fd) {
  assert(push_session(fd, WORK_STATUS_INITIAL) == 0);
  session_t *session = pop_session_for_read(fd);
  assert(session != NULL);
  return session;
}

// Plain connection that has been read and is now held by the "parser"
static session_t *st_hold_at_parser(int fd) {
  st_open(fd);
  assert(push_session(fd, WORK_STATUS_REQUEST_READ) == 0);
  session_t *session = pop_session(WORK_STATUS_REQUEST_READ);
  assert(session != NULL);
  assert(session->meta.related_fd == fd);
  return session;
}

// Plain connection held by the "sender"
static session_t *st_hold_at_sender(int fd) {
  session_t *session = st_hold_at_parser(fd);
  assert(push_session(fd, WORK_STATUS_PARSED) == 0);
  assert(pop_session(WORK_STATUS_PARSED) == session);
  assert(push_session(fd, WORK_STATUS_DATA_FETCHED) == 0);
  assert(pop_session(WORK_STATUS_DATA_FETCHED) == session);
  assert(push_session(fd, WORK_STATUS_READY_TO_SEND) == 0);
  assert(pop_session(WORK_STATUS_READY_TO_SEND) == session);
  return session;
}

// ---------------------------------------------------------------------------
// Init / destroy
// ---------------------------------------------------------------------------

void *init_destroy_session_test() {
  st_setup(SESSION_TEST_SIZE);
  st_teardown();

  return NULL;
}

void *reinit_session_cache_test() {
  // The cache must be re-creatable after being destroyed
  st_setup(SESSION_TEST_SIZE);
  st_open(1);
  st_teardown();

  st_setup(SESSION_TEST_SIZE);
  assert(pop_session_for_read(1) == NULL);
  st_teardown();

  return NULL;
}

// ---------------------------------------------------------------------------
// New connections and reading
// ---------------------------------------------------------------------------

void *initial_session_test() {
  st_setup(SESSION_TEST_SIZE);

  assert(push_session(1, WORK_STATUS_INITIAL) == 0);
  session_t *session = pop_session_for_read(1);
  assert(session != NULL);
  assert(session->meta.related_fd == 1);
  assert(session->meta.closing == 0);
  assert(session->request.uri[0][0] == 0);
  assert(session->request.is_ssl == 0);
  assert(session->buffer != NULL);
  assert(session->buffer->count == 0);
  assert(session->bio != NULL);
  assert(session->ssl == NULL);
  assert(session->failed_send_attempts == 0);

  st_teardown();

  return NULL;
}

void *initial_session_with_ssl_ctx_test() {
  // A plain connection on a cache that has an SSL context
  st_setup_ssl(SESSION_TEST_SIZE);

  session_t *session = st_open(1);
  assert(session->ssl != NULL);
  assert(session->bio != NULL);
  assert(session->request.is_ssl == 0);

  st_teardown();

  return NULL;
}

void *pop_for_read_unknown_fd_test() {
  st_setup(SESSION_TEST_SIZE);

  assert(push_session(1, WORK_STATUS_INITIAL) == 0);
  assert(pop_session_for_read(2) == NULL);
  // Session 1 is untouched
  assert(pop_session_for_read(1) != NULL);

  st_teardown();

  return NULL;
}

void *pop_for_read_no_sessions_test() {
  // An fd that has no idle session (e.g. all of them are in flight) must not
  // abort, the listener treats NULL as "nothing to read right now"
  st_setup(SESSION_TEST_SIZE);

  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *pop_for_read_twice_test() {
  st_setup(SESSION_TEST_SIZE);

  st_open(1);
  // The session is held by the first caller, it is no longer idle
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *idle_splice_tail_then_append_test() {
  // Removing the last idle session must update the tail of the list,
  // otherwise the next appended session is lost
  st_setup(SESSION_TEST_SIZE);

  for (int fd = 1; fd <= 3; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);

  assert(pop_session_for_read(3) != NULL);
  assert(push_session(4, WORK_STATUS_INITIAL) == 0);

  assert(pop_session_for_read(4) != NULL);
  assert(pop_session_for_read(1) != NULL);
  assert(pop_session_for_read(2) != NULL);
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *idle_splice_middle_test() {
  st_setup(SESSION_TEST_SIZE);

  for (int fd = 1; fd <= 3; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);

  assert(pop_session_for_read(2) != NULL);
  assert(push_session(4, WORK_STATUS_INITIAL) == 0);

  assert(pop_session_for_read(1) != NULL);
  assert(pop_session_for_read(3) != NULL);
  assert(pop_session_for_read(4) != NULL);
  assert(pop_session_for_read(2) == NULL);

  st_teardown();

  return NULL;
}

// ---------------------------------------------------------------------------
// Retry (not enough data yet): the session goes back to idle
// ---------------------------------------------------------------------------

void *retry_returns_session_to_idle_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *session = st_open(1);
  assert(push_session(1, WORK_STATUS_RETRY) == 0);

  // Same session, not a new one, and only once
  assert(pop_session_for_read(1) == session);
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *retry_keeps_buffered_data_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *session = st_open(1);
  assert(ENSURE_CAPACITY(session->buffer, 16) > 0);
  memcpy(session->buffer->data, "GET /", 5);
  session->buffer->count = 5;

  assert(push_session(1, WORK_STATUS_RETRY) == 0);

  session_t *again = pop_session_for_read(1);
  assert(again == session);
  assert(again->buffer->count == 5);
  assert(memcmp(again->buffer->data, "GET /", 5) == 0);

  st_teardown();

  return NULL;
}

void *retry_does_not_use_extra_slot_test() {
  st_setup(SESSION_TEST_SIZE);

  for (int fd = 1; fd <= SESSION_TEST_SIZE; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);

  assert(pop_session_for_read(1) != NULL);
  assert(push_session(1, WORK_STATUS_RETRY) == 0);

  // Cache is exactly as full as before the retry
  assert(push_session(SESSION_TEST_SIZE + 1, WORK_STATUS_INITIAL) == -1);

  st_teardown();

  return NULL;
}

// ---------------------------------------------------------------------------
// The pipeline: read -> parsed -> data fetched -> ready to send -> sent
// ---------------------------------------------------------------------------

void *full_pipeline_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *session = st_hold_at_sender(1);
  session->request.uri[0][0] = 'x';
  assert(ENSURE_CAPACITY(session->buffer, 16) > 0);
  session->buffer->count = 8;

  assert(push_session(1, WORK_STATUS_SENT) == 0);

  // Back to idle with the request cleared, ready for the next request
  session_t *idle = pop_session_for_read(1);
  assert(idle == session);
  assert(idle->meta.related_fd == 1);
  assert(idle->request.uri[0][0] == 0);
  assert(idle->buffer->count == 0);
  assert(is_marked(1) == 0);

  st_teardown();

  return NULL;
}

void *fifo_order_within_stage_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *sessions[3];
  for (int fd = 1; fd <= 3; fd++) {
    sessions[fd - 1] = st_open(fd);
    assert(push_session(fd, WORK_STATUS_REQUEST_READ) == 0);
  }

  for (int i = 0; i < 3; i++)
    assert(pop_session(WORK_STATUS_REQUEST_READ) == sessions[i]);

  st_teardown();

  return NULL;
}

void *stages_are_independent_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *a = st_open(1);
  session_t *b = st_open(2);
  assert(push_session(1, WORK_STATUS_REQUEST_READ) == 0);
  assert(push_session(2, WORK_STATUS_REQUEST_READ) == 0);

  assert(pop_session(WORK_STATUS_REQUEST_READ) == a);
  assert(push_session(1, WORK_STATUS_PARSED) == 0);

  // Session 1 is parsed while session 2 is still waiting for the parser
  assert(pop_session(WORK_STATUS_PARSED) == a);
  assert(pop_session(WORK_STATUS_REQUEST_READ) == b);

  st_teardown();

  return NULL;
}

void *sent_resets_failed_attempts_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *session = st_hold_at_sender(1);

  for (int i = 0; i < 2; i++) {
    assert(push_session(1, WORK_STATUS_SEND_FAILED) == 0);
    assert(pop_session(WORK_STATUS_READY_TO_SEND) == session);
  }
  assert(session->failed_send_attempts == 2);

  assert(push_session(1, WORK_STATUS_SENT) == 0);
  assert(session->failed_send_attempts == 0);
  assert(pop_session_for_read(1) == session);

  st_teardown();

  return NULL;
}

void *send_failed_three_times_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *session = st_hold_at_sender(1);

  // The first two failures are retried
  for (int i = 0; i < 2; i++) {
    assert(push_session(1, WORK_STATUS_SEND_FAILED) == 0);
    assert(is_marked(1) == 0);
    assert(pop_session(WORK_STATUS_READY_TO_SEND) == session);
  }

  // The third one gives up on the connection
  assert(push_session(1, WORK_STATUS_SEND_FAILED) == 0);
  assert(is_marked(1) == 1);
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

// ---------------------------------------------------------------------------
// Rejecting and capacity
// ---------------------------------------------------------------------------

void *rejected_releases_session_test() {
  st_setup(SESSION_TEST_SIZE);

  st_open(1);
  assert(push_session(1, WORK_STATUS_REJECTED) == 0);

  assert(is_marked(1) == 1);
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *high_fd_session_test() {
  // fds are not bounded by the number of sessions, only by MAX_FD
  st_setup(SESSION_TEST_SIZE);

  int fd = MAX_FD - 1;
  assert(push_session(fd, WORK_STATUS_INITIAL) == 0);
  session_t *session = pop_session_for_read(fd);
  assert(session != NULL);
  assert(session->meta.related_fd == fd);
  assert(push_session(fd, WORK_STATUS_REJECTED) == 0);
  assert(is_marked(fd) == 1);

  st_teardown();

  return NULL;
}

void *rejected_unknown_fd_test() {
  st_setup(SESSION_TEST_SIZE);

  assert(push_session(1, WORK_STATUS_REJECTED) == 0);
  assert(is_marked(1) == 0);

  st_teardown();

  return NULL;
}

void *capacity_exhausted_test() {
  st_setup(SESSION_TEST_SIZE);

  for (int fd = 1; fd <= SESSION_TEST_SIZE; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);

  // No free slot: the connection is refused and its fd marked so the listener
  // drops it, the existing sessions are not touched
  int extra = SESSION_TEST_SIZE + 1;
  assert(push_session(extra, WORK_STATUS_INITIAL) == -1);
  assert(is_marked(extra) == 1);
  assert(pop_session_for_read(extra) == NULL);

  for (int fd = 1; fd <= SESSION_TEST_SIZE; fd++) {
    assert(is_marked(fd) == 0);
    session_t *session = pop_session_for_read(fd);
    assert(session != NULL);
    assert(session->meta.related_fd == fd);
  }

  st_teardown();

  return NULL;
}

void *slot_reused_after_rejected_test() {
  st_setup(SESSION_TEST_SIZE);

  for (int fd = 1; fd <= SESSION_TEST_SIZE; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);

  assert(pop_session_for_read(2) != NULL);
  assert(push_session(2, WORK_STATUS_REJECTED) == 0);

  int extra = SESSION_TEST_SIZE + 1;
  assert(push_session(extra, WORK_STATUS_INITIAL) == 0);
  session_t *session = pop_session_for_read(extra);
  assert(session != NULL);
  assert(session->meta.related_fd == extra);

  st_teardown();

  return NULL;
}

void *every_slot_reusable_many_times_test() {
  st_setup(SESSION_TEST_SIZE);

  // Connect and drop far more connections than there are slots
  for (int round = 0; round < 10 * SESSION_TEST_SIZE; round++) {
    int fd = 1 + (round % SESSION_TEST_SIZE);
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);
    session_t *session = pop_session_for_read(fd);
    assert(session != NULL);
    assert(session->meta.related_fd == fd);
    assert(session->request.uri[0][0] == 0);
    assert(session->buffer->count == 0);
    assert(push_session(fd, WORK_STATUS_REJECTED) == 0);
  }

  st_teardown();

  return NULL;
}

// ---------------------------------------------------------------------------
// request_close_session
// ---------------------------------------------------------------------------

void *request_close_idle_test() {
  st_setup(SESSION_TEST_SIZE);

  assert(push_session(1, WORK_STATUS_INITIAL) == 0);
  request_close_session(1);

  // Idle sessions are released right away
  assert(is_marked(1) == 1);
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *request_close_idle_frees_slot_test() {
  st_setup(SESSION_TEST_SIZE);

  for (int fd = 1; fd <= SESSION_TEST_SIZE; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);

  request_close_session(3);

  int extra = SESSION_TEST_SIZE + 1;
  assert(push_session(extra, WORK_STATUS_INITIAL) == 0);
  assert(pop_session_for_read(extra) != NULL);

  st_teardown();

  return NULL;
}

void *request_close_unknown_fd_test() {
  st_setup(SESSION_TEST_SIZE);

  st_open(1);
  request_close_session(2);

  assert(is_marked(1) == 0);
  assert(is_marked(2) == 0);

  st_teardown();

  return NULL;
}

void *request_close_twice_test() {
  // The listener keeps getting POLLHUP until the fd is closed
  st_setup(SESSION_TEST_SIZE);

  assert(push_session(1, WORK_STATUS_INITIAL) == 0);
  request_close_session(1);
  request_close_session(1);

  assert(is_marked(1) == 1);
  assert(pop_session_for_read(1) == NULL);

  // The slot was released once, not twice
  for (int fd = 2; fd <= SESSION_TEST_SIZE + 1; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);
  assert(push_session(SESSION_TEST_SIZE + 2, WORK_STATUS_INITIAL) == -1);

  st_teardown();

  return NULL;
}

void *request_close_held_session_is_deferred_test() {
  st_setup(SESSION_TEST_SIZE);

  session_t *session = st_hold_at_parser(1);
  request_close_session(1);

  // A worker still owns the session: it must stay intact and the fd must not
  // be handed to the listener for closing yet
  assert(session->meta.related_fd == 1);
  assert(session->meta.closing == 1);
  assert(is_marked(1) == 0);

  // It is released when the worker hands it back
  assert(push_session(1, WORK_STATUS_PARSED) == 0);
  assert(is_marked(1) == 1);
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *request_close_queued_session_is_deferred_test() {
  st_setup(SESSION_TEST_SIZE);

  st_open(1);
  assert(push_session(1, WORK_STATUS_REQUEST_READ) == 0);
  request_close_session(1);
  assert(is_marked(1) == 0);

  // Still queued, so a worker will pick it up and hand it back
  session_t *session = pop_session(WORK_STATUS_REQUEST_READ);
  assert(session != NULL);
  assert(session->meta.closing == 1);
  assert(push_session(1, WORK_STATUS_PARSED) == 0);
  assert(is_marked(1) == 1);

  st_teardown();

  return NULL;
}

void *closing_session_released_on_sent_test() {
  // A closing session must never go back to idle, nobody would read it again
  st_setup(SESSION_TEST_SIZE);

  st_hold_at_sender(1);
  request_close_session(1);
  assert(push_session(1, WORK_STATUS_SENT) == 0);

  assert(is_marked(1) == 1);
  assert(pop_session_for_read(1) == NULL);

  st_teardown();

  return NULL;
}

void *closing_session_released_on_send_failed_test() {
  st_setup(SESSION_TEST_SIZE);

  st_hold_at_sender(1);
  request_close_session(1);
  // No retries for a connection that is going away
  assert(push_session(1, WORK_STATUS_SEND_FAILED) == 0);

  assert(is_marked(1) == 1);

  st_teardown();

  return NULL;
}

void *closing_flag_cleared_on_reuse_test() {
  st_setup(SESSION_TEST_SIZE);

  st_hold_at_parser(1);
  request_close_session(1);
  assert(push_session(1, WORK_STATUS_PARSED) == 0);

  // The next connection that gets this slot (and fd number) starts clean
  session_t *session = st_open(1);
  assert(session->meta.closing == 0);
  assert(push_session(1, WORK_STATUS_REQUEST_READ) == 0);
  assert(pop_session(WORK_STATUS_REQUEST_READ) == session);
  assert(push_session(1, WORK_STATUS_PARSED) == 0);
  assert(pop_session(WORK_STATUS_PARSED) == session);

  st_teardown();

  return NULL;
}

// ---------------------------------------------------------------------------
// SSL
// ---------------------------------------------------------------------------

void *ssl_handshake_in_progress_test() {
  st_setup_ssl(SESSION_TEST_SIZE);

  int sv[2];
  st_socketpair(sv);

  // Client has not said anything yet: the handshake needs more data
  assert(push_session(sv[0], WORK_STATUS_INITIAL_SSL) == 0);
  session_t *session = pop_session_for_read(sv[0]);
  assert(session != NULL);
  assert(session->request.is_ssl == 1);
  assert(session->ssl != NULL);
  assert(is_marked(sv[0]) == 0);

  st_teardown();
  close(sv[0]);
  close(sv[1]);

  return NULL;
}

void *ssl_port_receives_plain_http_test() {
  st_setup_ssl(SESSION_TEST_SIZE);

  int sv[2];
  st_socketpair(sv);

  const char *request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  assert(write(sv[1], request, strlen(request)) == (ssize_t)strlen(request));

  // The handshake fails: the session is released and must not be queued
  assert(push_session(sv[0], WORK_STATUS_INITIAL_SSL) == -1);
  assert(is_marked(sv[0]) == 1);
  assert(pop_session_for_read(sv[0]) == NULL);

  // Its slot is free again and the lists are intact
  for (int fd = 1; fd <= 3; fd++)
    assert(push_session(100 + fd, WORK_STATUS_INITIAL) == 0);
  for (int fd = 1; fd <= 3; fd++) {
    session_t *session = pop_session_for_read(100 + fd);
    assert(session != NULL);
    assert(session->meta.related_fd == 100 + fd);
  }

  st_teardown();
  close(sv[0]);
  close(sv[1]);

  return NULL;
}

void *is_ssl_cleared_when_slot_reused_test() {
  st_setup_ssl(SESSION_TEST_SIZE);

  int sv[2];
  st_socketpair(sv);

  assert(push_session(sv[0], WORK_STATUS_INITIAL_SSL) == 0);
  session_t *session = pop_session_for_read(sv[0]);
  assert(session != NULL);
  assert(session->request.is_ssl == 1);
  assert(push_session(sv[0], WORK_STATUS_REJECTED) == 0);

  // A plain HTTP connection on the recycled slot is not SSL
  assert(push_session(sv[0], WORK_STATUS_INITIAL) == 0);
  session = pop_session_for_read(sv[0]);
  assert(session != NULL);
  assert(session->request.is_ssl == 0);

  st_teardown();
  close(sv[0]);
  close(sv[1]);

  return NULL;
}

// ---------------------------------------------------------------------------
// Threads
// ---------------------------------------------------------------------------

static void *st_pop_stage_thread(void *arg) {
  WORK_STATUS status = *(WORK_STATUS *)arg;
  return pop_session(status);
}

void *pop_session_blocks_until_pushed_test() {
  st_setup(SESSION_TEST_SIZE);

  WORK_STATUS status = WORK_STATUS_REQUEST_READ;
  pthread_t thread;
  assert(pthread_create(&thread, NULL, st_pop_stage_thread, &status) == 0);

  // Let the worker reach pthread_cond_wait
  usleep(50 * 1000);

  session_t *session = st_open(1);
  assert(push_session(1, WORK_STATUS_REQUEST_READ) == 0);

  void *popped = NULL;
  assert(pthread_join(thread, &popped) == 0);
  assert(popped == session);

  st_teardown();

  return NULL;
}

typedef struct {
  WORK_STATUS in;
  WORK_STATUS out;
  int iterations;
} st_stage_t;

static void *st_stage_worker(void *arg) {
  st_stage_t *stage = (st_stage_t *)arg;
  for (int i = 0; i < stage->iterations; i++) {
    session_t *session = pop_session(stage->in);
    assert(session != NULL);
    assert(push_session(session->meta.related_fd, stage->out) == 0);
  }
  return NULL;
}

void *concurrent_pipeline_test() {
  // The same shape as the real server: one thread per stage handing sessions
  // on, the listener (this thread) starting and finishing every request
  const int rounds = 25;
  const int sessions = SESSION_TEST_SIZE;
  st_setup(SESSION_TEST_SIZE);

  st_stage_t stages[3] = {
      {WORK_STATUS_REQUEST_READ, WORK_STATUS_PARSED, rounds * sessions},
      {WORK_STATUS_PARSED, WORK_STATUS_DATA_FETCHED, rounds * sessions},
      {WORK_STATUS_DATA_FETCHED, WORK_STATUS_READY_TO_SEND, rounds * sessions},
  };
  pthread_t threads[3];
  for (int i = 0; i < 3; i++)
    assert(pthread_create(&threads[i], NULL, st_stage_worker, &stages[i]) == 0);

  for (int fd = 1; fd <= sessions; fd++)
    assert(push_session(fd, WORK_STATUS_INITIAL) == 0);

  for (int round = 0; round < rounds; round++) {
    for (int fd = 1; fd <= sessions; fd++) {
      session_t *session = pop_session_for_read(fd);
      assert(session != NULL);
      assert(session->meta.related_fd == fd);
      assert(push_session(fd, WORK_STATUS_REQUEST_READ) == 0);
    }

    // Every session comes out of the last stage exactly once per round
    int seen[SESSION_TEST_SIZE + 1] = {0};
    for (int i = 0; i < sessions; i++) {
      session_t *session = pop_session(WORK_STATUS_READY_TO_SEND);
      assert(session != NULL);
      int fd = session->meta.related_fd;
      assert(fd >= 1 && fd <= sessions);
      assert(++seen[fd] == 1);
      assert(push_session(fd, WORK_STATUS_SENT) == 0);
    }
  }

  for (int i = 0; i < 3; i++)
    assert(pthread_join(threads[i], NULL) == 0);

  for (int fd = 1; fd <= sessions; fd++)
    assert(pop_session_for_read(fd) != NULL);

  st_teardown();

  return NULL;
}

int session_test() {
  // A hung test (e.g. a worker waiting on a session that never arrives) would
  // otherwise block CI until it times out
  alarm(120);

  session_start_case(init_destroy_session_test, "init_destroy_session_test");
  session_start_case(reinit_session_cache_test, "reinit_session_cache_test");

  session_start_case(initial_session_test, "initial_session_test");
  session_start_case(initial_session_with_ssl_ctx_test,
                     "initial_session_with_ssl_ctx_test");
  session_start_case(pop_for_read_unknown_fd_test,
                     "pop_for_read_unknown_fd_test");
  session_start_case(pop_for_read_no_sessions_test,
                     "pop_for_read_no_sessions_test");
  session_start_case(pop_for_read_twice_test, "pop_for_read_twice_test");
  session_start_case(idle_splice_tail_then_append_test,
                     "idle_splice_tail_then_append_test");
  session_start_case(idle_splice_middle_test, "idle_splice_middle_test");

  session_start_case(retry_returns_session_to_idle_test,
                     "retry_returns_session_to_idle_test");
  session_start_case(retry_keeps_buffered_data_test,
                     "retry_keeps_buffered_data_test");
  session_start_case(retry_does_not_use_extra_slot_test,
                     "retry_does_not_use_extra_slot_test");

  session_start_case(full_pipeline_test, "full_pipeline_test");
  session_start_case(fifo_order_within_stage_test,
                     "fifo_order_within_stage_test");
  session_start_case(stages_are_independent_test,
                     "stages_are_independent_test");
  session_start_case(sent_resets_failed_attempts_test,
                     "sent_resets_failed_attempts_test");
  session_start_case(send_failed_three_times_test,
                     "send_failed_three_times_test");

  session_start_case(rejected_releases_session_test,
                     "rejected_releases_session_test");
  session_start_case(rejected_unknown_fd_test, "rejected_unknown_fd_test");
  session_start_case(high_fd_session_test, "high_fd_session_test");
  session_start_case(capacity_exhausted_test, "capacity_exhausted_test");
  session_start_case(slot_reused_after_rejected_test,
                     "slot_reused_after_rejected_test");
  session_start_case(every_slot_reusable_many_times_test,
                     "every_slot_reusable_many_times_test");

  session_start_case(request_close_idle_test, "request_close_idle_test");
  session_start_case(request_close_idle_frees_slot_test,
                     "request_close_idle_frees_slot_test");
  session_start_case(request_close_unknown_fd_test,
                     "request_close_unknown_fd_test");
  session_start_case(request_close_twice_test, "request_close_twice_test");
  session_start_case(request_close_held_session_is_deferred_test,
                     "request_close_held_session_is_deferred_test");
  session_start_case(request_close_queued_session_is_deferred_test,
                     "request_close_queued_session_is_deferred_test");
  session_start_case(closing_session_released_on_sent_test,
                     "closing_session_released_on_sent_test");
  session_start_case(closing_session_released_on_send_failed_test,
                     "closing_session_released_on_send_failed_test");
  session_start_case(closing_flag_cleared_on_reuse_test,
                     "closing_flag_cleared_on_reuse_test");

  session_start_case(ssl_handshake_in_progress_test,
                     "ssl_handshake_in_progress_test");
  session_start_case(ssl_port_receives_plain_http_test,
                     "ssl_port_receives_plain_http_test");
  session_start_case(is_ssl_cleared_when_slot_reused_test,
                     "is_ssl_cleared_when_slot_reused_test");

  session_start_case(pop_session_blocks_until_pushed_test,
                     "pop_session_blocks_until_pushed_test");
  session_start_case(concurrent_pipeline_test, "concurrent_pipeline_test");

  alarm(0);

  return session_test_count;
}

static int session_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", session_test_count++, name);

  func(NULL);

  return 0;
}
