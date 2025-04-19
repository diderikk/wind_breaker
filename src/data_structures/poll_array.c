#include "poll_array.h"
#include "../properties.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include <unistd.h>

int count;
struct pollfd *fds;
static int max_count = 0;
static int last_in_index;
static volatile int* marks = NULL;

// Initialize the poll array with the listen_fd
// This should be called once, when the server fronts
void init_poll_array(int listen_fd) {
  assert(listen_fd > 0);
  assert(listen_fd < 16384);
  assert(fds == NULL);
  assert(marks == NULL);
  max_count = get_session_max_size();

  assert(max_count > 0);
  assert(max_count < 16384);

  fds = calloc(max_count, sizeof(struct pollfd));
  assert(fds != NULL);

  marks = calloc(max_count, sizeof(int));

  log_info("Initialized poll array with count %d", max_count);
  fds[0].fd = listen_fd;
  fds[0].events = POLLIN; // Report ready to read on incoming connection
  count = 1;
  last_in_index = 1;
}

void destroy_poll_array() {
  free(fds);
  fds = NULL;
  free((int*)marks);
  marks = NULL;
  assert(fds == NULL);
  assert(marks == NULL);
}

void mark(int fd) {
  assert(fd > 0);
  assert(fd < 16384);
  assert(fds != NULL);
  assert(marks != NULL);

  for (int i = 0; i < max_count; i++) {
    if (fds[i].fd == fd) {
      __sync_fetch_and_add(&marks[i], 1);
      break;
    }
  }
}

char is_marked(int fd) {
  assert(fd > 0);
  assert(fd < 16384);
  assert(fds != NULL);
  assert(marks != NULL);

  for (int i = 0; i < max_count; i++) {
    if (fds[i].fd == fd) {
      return __sync_fetch_and_add(&marks[i], 0) > 0;
    }
  }
  return 0;
}

static inline int peek_next() {
  assert(fds != NULL);
  assert(marks != NULL);

  if (count < max_count)
    return count;
  else {
    // If none available, find a socket not expecting any events
    for (int i = last_in_index; i < max_count; i++) {
      if (!(fds[i].fd & (POLLIN | POLLOUT))) {
        struct pollfd *temp = &fds[i];
        fds[i] = fds[last_in_index];
        fds[last_in_index] = *temp;
      }
    }
  }
  return last_in_index;
}

int add_poll_fd_sync(int fd) {
  assert(fd > 0);
  assert(fd < 16384);
  assert(fds != NULL);
  assert(marks != NULL);
  log_trace("Poller: Adding fd %d", fd);

  int next = peek_next();
  assert(next < max_count);
  // Dont overwrite the listen_fd
  if (next == 0)
    next++;

  fds[next].fd = fd;
  fds[next].events = POLLIN; // Check ready-to-read
  fds[next].revents = 0;

  if (count < max_count)
    count++;
  else {
    last_in_index = (last_in_index + 1) % max_count;
    // Dont overwrite the listen_fd
    if (last_in_index == 0)
      last_in_index++;
  }

  log_trace("Added new socket %d", fd);
  assert(count <= max_count);
  assert(count > 1);
  assert(last_in_index < max_count);
  assert(last_in_index > 0);
  return next;
}

void remove_poll_fd_by_index_sync(int *i) {
  assert(*i > 0);
  assert(*i < max_count);
  assert(fds != NULL);
  assert(marks != NULL);
  assert(*i < count);

  log_trace("Removing fd %d", fds[*i].fd);

  close(fds[*i].fd);

  for (int j = *i; j < count - 1; j++) {
    fds[j].fd = fds[(j + 1)].fd;
    fds[j].events = fds[(j + 1)].events;
    fds[j].revents = fds[(j + 1)].revents;
    __sync_fetch_and_and(&marks[j], __sync_fetch_and_add(&marks[j + 1], 0));
  }
  fds[count - 1].fd = 0;
  fds[count - 1].events = 0;
  fds[count - 1].revents = 0;
  __sync_fetch_and_and(&marks[count - 1], 0);

  if (*i < last_in_index)
    last_in_index--;
  count--;
  (*i)--;

  log_trace("Removed fd, new count is %d", count);
  assert(count > 0);
  assert(last_in_index > 0);
}
