#include <poll.h>

#include "../../../src/data_structures/marked_fds.h"
#include "../../../src/data_structures/poll_array.h"
#include "../../../src/properties.h"
#include "../../../src/utils/assert2.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

static int poll_array_test_count = 0;

#define PA_DEFAULT_MAX 64
#define PA_MAX_FDS 128

static int poll_array_start_case(void *(*func)(void *), const char *name);

// remove_poll_fd_by_index_sync() close()s the fd, so the tests must hand out
// real file descriptors instead of numbers like 2 (stderr).
static int pa_open_fd(void) {
  int fd = open("/dev/null", O_RDONLY);
  assert(fd > 0);
  return fd;
}

static int pa_is_closed(int fd) {
  return fcntl(fd, F_GETFD) == -1 && errno == EBADF;
}

// Close every fd still held by the poll array, then destroy it
static void pa_teardown(void) {
  struct pollfd *fds = get_poll_fd_array();
  int count = get_poll_fd_array_count();
  for (int i = 0; i < count; i++)
    close(fds[i].fd);
  destroy_poll_array();
}

// Returns the listener fd
static int pa_setup(int max_size) {
  set_session_max_size(max_size);
  int listener = pa_open_fd();
  init_poll_array(listener);
  return listener;
}

void *init_poll_array_test() {
  int listener = pa_setup(PA_DEFAULT_MAX);

  assert(get_poll_fd_array() != NULL);
  assert(get_poll_fd_array_count() == 1);
  assert(get_poll_fd_array()[0].fd == listener);
  assert(get_poll_fd_array()[0].events == POLLIN);

  pa_teardown();
  return NULL;
}

void *destroy_poll_array_test() {
  pa_setup(PA_DEFAULT_MAX);
  pa_teardown();

  assert(get_poll_fd_array() == NULL);
  assert(get_poll_fd_array_count() == 0);

  return NULL;
}

// The array can be initialised again after a destroy
void *reinit_poll_array_test() {
  pa_setup(PA_DEFAULT_MAX);
  assert(add_poll_fd_sync(pa_open_fd()) >= 0);
  pa_teardown();

  int listener = pa_setup(PA_DEFAULT_MAX);
  assert(get_poll_fd_array_count() == 1);
  assert(get_poll_fd_array()[0].fd == listener);

  pa_teardown();
  return NULL;
}

void *add_poll_fd_sync_test() {
  int listener = pa_setup(PA_DEFAULT_MAX);
  int a = pa_open_fd();
  int b = pa_open_fd();

  // The return value is the slot the fd was written to
  assert(add_poll_fd_sync(a) == 1);
  assert(get_poll_fd_array_count() == 2);
  assert(get_poll_fd_array()[1].fd == a);
  assert(get_poll_fd_array()[1].events == POLLIN);
  assert(get_poll_fd_array()[1].revents == 0);

  assert(add_poll_fd_sync(b) == 2);
  assert(get_poll_fd_array_count() == 3);
  assert(get_poll_fd_array()[2].fd == b);
  assert(get_poll_fd_array()[2].events == POLLIN);
  assert(get_poll_fd_array()[2].revents == 0);

  // Listener untouched
  assert(get_poll_fd_array()[0].fd == listener);

  pa_teardown();
  return NULL;
}

// Adding past the initial capacity (16) and past one doubling (32) must keep
// every earlier entry intact, including the listener
void *add_poll_fd_sync_growth_test() {
  int listener = pa_setup(PA_DEFAULT_MAX);
  int added[PA_MAX_FDS];
  int n = 40;

  for (int i = 0; i < n; i++) {
    added[i] = pa_open_fd();
    assert(add_poll_fd_sync(added[i]) == i + 1);
  }

  assert(get_poll_fd_array_count() == n + 1);
  assert(get_poll_fd_array()[0].fd == listener);
  for (int i = 0; i < n; i++) {
    assert(get_poll_fd_array()[i + 1].fd == added[i]);
    assert(get_poll_fd_array()[i + 1].events == POLLIN);
  }

  pa_teardown();
  return NULL;
}

// The array holds the listener plus up to session max size clients.
// The add after that is refused and leaves the array unchanged.
void *add_poll_fd_sync_limit_test() {
  int max = 10;
  pa_setup(max);

  for (int i = 0; i < max; i++)
    assert(add_poll_fd_sync(pa_open_fd()) >= 0);
  assert(get_poll_fd_array_count() == max + 1); // clients + listener

  int rejected = pa_open_fd();
  assert(add_poll_fd_sync(rejected) == -1);
  assert(get_poll_fd_array_count() == max + 1);
  for (int i = 0; i < get_poll_fd_array_count(); i++)
    assert(get_poll_fd_array()[i].fd != rejected);

  close(rejected);
  pa_teardown();
  return NULL;
}

// Same as above, but the max is not a power of two and lies above the initial
// capacity, so the growth step has to be capped (16 -> 21, not 32)
void *add_poll_fd_sync_limit_after_growth_test() {
  int max = 20;
  pa_setup(max);

  for (int i = 0; i < max; i++)
    assert(add_poll_fd_sync(pa_open_fd()) >= 0);
  assert(get_poll_fd_array_count() == max + 1);

  int rejected = pa_open_fd();
  assert(add_poll_fd_sync(rejected) == -1);
  assert(get_poll_fd_array_count() == max + 1);

  close(rejected);
  pa_teardown();
  return NULL;
}

// A rejected add must not corrupt the array: a later remove frees a slot
// that can be used again
void *add_poll_fd_sync_after_limit_and_remove_test() {
  int max = 5;
  pa_setup(max);

  for (int i = 0; i < max; i++)
    assert(add_poll_fd_sync(pa_open_fd()) >= 0);

  int rejected = pa_open_fd();
  assert(add_poll_fd_sync(rejected) == -1);

  remove_poll_fd_by_index_sync(1);
  assert(get_poll_fd_array_count() == max);

  assert(add_poll_fd_sync(rejected) == max);
  assert(get_poll_fd_array_count() == max + 1);
  assert(get_poll_fd_array()[max].fd == rejected);

  pa_teardown();
  return NULL;
}

// Removing from the middle moves the last entry into the freed slot
void *remove_poll_fd_by_index_sync_middle_test() {
  int listener = pa_setup(PA_DEFAULT_MAX);
  int a = pa_open_fd();
  int b = pa_open_fd();
  int c = pa_open_fd();
  add_poll_fd_sync(a);
  add_poll_fd_sync(b);
  add_poll_fd_sync(c);

  remove_poll_fd_by_index_sync(1); // removes a

  assert(get_poll_fd_array_count() == 3);
  assert(get_poll_fd_array()[0].fd == listener);
  assert(get_poll_fd_array()[1].fd == c); // last moved into the hole
  assert(get_poll_fd_array()[2].fd == b);
  assert(get_poll_fd_array()[1].events == POLLIN);
  assert(pa_is_closed(a));
  assert(!pa_is_closed(b));
  assert(!pa_is_closed(c));

  pa_teardown();
  return NULL;
}

// Removing the last entry (the swap copies the entry onto itself)
void *remove_poll_fd_by_index_sync_last_test() {
  pa_setup(PA_DEFAULT_MAX);
  int a = pa_open_fd();
  int b = pa_open_fd();
  add_poll_fd_sync(a);
  add_poll_fd_sync(b);

  remove_poll_fd_by_index_sync(2);

  assert(get_poll_fd_array_count() == 2);
  assert(get_poll_fd_array()[1].fd == a);
  assert(pa_is_closed(b));
  assert(!pa_is_closed(a));
  // Slot beyond count is cleared
  assert(get_poll_fd_array()[2].fd == 0);
  assert(get_poll_fd_array()[2].events == 0);
  assert(get_poll_fd_array()[2].revents == 0);

  pa_teardown();
  return NULL;
}

// Removing the only client leaves just the listener
void *remove_poll_fd_by_index_sync_only_client_test() {
  int listener = pa_setup(PA_DEFAULT_MAX);
  int a = pa_open_fd();
  add_poll_fd_sync(a);

  remove_poll_fd_by_index_sync(1);

  assert(get_poll_fd_array_count() == 1);
  assert(get_poll_fd_array()[0].fd == listener);
  assert(pa_is_closed(a));
  assert(!pa_is_closed(listener));

  pa_teardown();
  return NULL;
}

// The swapped-in entry keeps its own state (events/revents come along)
void *remove_poll_fd_by_index_sync_keeps_moved_state_test() {
  pa_setup(PA_DEFAULT_MAX);
  int a = pa_open_fd();
  int b = pa_open_fd();
  add_poll_fd_sync(a);
  add_poll_fd_sync(b);

  get_poll_fd_array()[2].events = POLLIN | POLLOUT;
  get_poll_fd_array()[2].revents = POLLOUT;

  remove_poll_fd_by_index_sync(1);

  assert(get_poll_fd_array()[1].fd == b);
  assert(get_poll_fd_array()[1].events == (POLLIN | POLLOUT));
  assert(get_poll_fd_array()[1].revents == POLLOUT);

  pa_teardown();
  return NULL;
}

// Removing a fd also clears its mark so a reused fd number starts clean
void *remove_poll_fd_by_index_sync_unmarks_test() {
  pa_setup(PA_DEFAULT_MAX);
  int a = pa_open_fd();
  int b = pa_open_fd();
  add_poll_fd_sync(a);
  add_poll_fd_sync(b);

  mark(a);
  mark(b);
  remove_poll_fd_by_index_sync(1);

  assert(is_marked(a) == 0);
  assert(is_marked(b) == 1); // untouched

  pa_teardown();
  return NULL;
}

// A freed slot is reused by the next add, and nothing is left behind
void *add_poll_fd_sync_after_remove_test() {
  pa_setup(PA_DEFAULT_MAX);
  int a = pa_open_fd();
  int b = pa_open_fd();
  int c = pa_open_fd();
  add_poll_fd_sync(a);
  add_poll_fd_sync(b);
  add_poll_fd_sync(c);

  remove_poll_fd_by_index_sync(1);
  assert(get_poll_fd_array_count() == 3);

  int d = pa_open_fd();
  assert(add_poll_fd_sync(d) == 3);
  assert(get_poll_fd_array_count() == 4);
  assert(get_poll_fd_array()[3].fd == d);
  assert(get_poll_fd_array()[3].revents == 0);

  pa_teardown();
  return NULL;
}

// Remove every client, newest first, as a drain
void *remove_poll_fd_by_index_sync_all_test() {
  int listener = pa_setup(PA_DEFAULT_MAX);
  int n = 20; // crosses the initial capacity
  int added[PA_MAX_FDS];

  for (int i = 0; i < n; i++) {
    added[i] = pa_open_fd();
    add_poll_fd_sync(added[i]);
  }

  for (int i = get_poll_fd_array_count() - 1; i > 0; i--)
    remove_poll_fd_by_index_sync(i);

  assert(get_poll_fd_array_count() == 1);
  assert(get_poll_fd_array()[0].fd == listener);
  for (int i = 0; i < n; i++)
    assert(pa_is_closed(added[i]));

  pa_teardown();
  return NULL;
}

// Mirrors how listener.c uses the array: walk from the back and remove marked
// entries while iterating. Every surviving client must be visited exactly
// once and no marked client may survive.
void *remove_poll_fd_by_index_sync_backwards_loop_test() {
  int listener = pa_setup(PA_DEFAULT_MAX);
  int n = 12;
  int added[PA_MAX_FDS];
  int visited[PA_MAX_FDS] = {0};

  for (int i = 0; i < n; i++) {
    added[i] = pa_open_fd();
    add_poll_fd_sync(added[i]);
    if (i % 2 == 0)
      mark(added[i]); // even ones are to be removed
  }

  int visits = 0;
  for (int i = get_poll_fd_array_count() - 1; i >= 0; i--) {
    int fd = get_poll_fd_array()[i].fd;
    visits++;
    visited[fd]++;
    if (is_marked(fd)) {
      remove_poll_fd_by_index_sync(i);
      continue;
    }
  }

  // listener + every client seen exactly once
  assert(visits == n + 1);
  assert(visited[listener] == 1);
  for (int i = 0; i < n; i++)
    assert(visited[added[i]] == 1);

  assert(get_poll_fd_array_count() == 1 + n / 2);
  assert(get_poll_fd_array()[0].fd == listener);
  for (int i = 1; i < get_poll_fd_array_count(); i++) {
    assert(!is_marked(get_poll_fd_array()[i].fd));
  }
  for (int i = 0; i < n; i++) {
    if (i % 2 == 0)
      assert(pa_is_closed(added[i]));
    else
      assert(!pa_is_closed(added[i]));
  }

  pa_teardown();
  return NULL;
}

int poll_array_test() {
  init_marked_fds();

  poll_array_start_case(init_poll_array_test, "init_poll_array_test");
  poll_array_start_case(destroy_poll_array_test, "destroy_poll_array_test");
  poll_array_start_case(reinit_poll_array_test, "reinit_poll_array_test");
  poll_array_start_case(add_poll_fd_sync_test, "add_poll_fd_sync_test");
  poll_array_start_case(add_poll_fd_sync_growth_test,
                        "add_poll_fd_sync_growth_test");
  poll_array_start_case(add_poll_fd_sync_limit_test,
                        "add_poll_fd_sync_limit_test");
  poll_array_start_case(add_poll_fd_sync_limit_after_growth_test,
                        "add_poll_fd_sync_limit_after_growth_test");
  poll_array_start_case(add_poll_fd_sync_after_limit_and_remove_test,
                        "add_poll_fd_sync_after_limit_and_remove_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_middle_test,
                        "remove_poll_fd_by_index_sync_middle_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_last_test,
                        "remove_poll_fd_by_index_sync_last_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_only_client_test,
                        "remove_poll_fd_by_index_sync_only_client_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_keeps_moved_state_test,
                        "remove_poll_fd_by_index_sync_keeps_moved_state_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_unmarks_test,
                        "remove_poll_fd_by_index_sync_unmarks_test");
  poll_array_start_case(add_poll_fd_sync_after_remove_test,
                        "add_poll_fd_sync_after_remove_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_all_test,
                        "remove_poll_fd_by_index_sync_all_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_backwards_loop_test,
                        "remove_poll_fd_by_index_sync_backwards_loop_test");

  destroy_marked_fds();
  return poll_array_test_count;
}

static int poll_array_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", poll_array_test_count++, name);

  func(NULL);

  return 0;
}
