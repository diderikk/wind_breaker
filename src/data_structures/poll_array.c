#include "poll_array.h"
#include "../properties.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include <stdlib.h>
#include <unistd.h>

static int max_count = 0;

// Initialize the poll array with the listen_fd
// This should be called once, when the server fronts
void init_poll_array(poll_array **pa, int listen_fd) {
  assert(listen_fd > 0);
  assert(listen_fd < 16384);
  assert(*pa == NULL);
  max_count = get_poll_array_max_size();

  assert(max_count > 0);
  assert(max_count < 16384);

  *pa = calloc(1, sizeof(**pa));
  assert(*pa != NULL);

  (*pa)->fds = calloc(max_count, sizeof(struct pollfd));
  assert((*pa)->fds != NULL);

  log_info("Initialized poll array with count %d", max_count);
  (*pa)->fds[0].fd = listen_fd;
  (*pa)->fds[0].events = POLLIN; // Report ready to read on incoming connection
  (*pa)->count = 1;
  (*pa)->last_in_index = 1;
}

void destroy_poll_array(poll_array **pa) {
  assert(*pa != NULL);
  free((*pa)->fds);
  (*pa)->fds = NULL;
  free(*pa);
  *pa = NULL;
  assert(*pa == NULL);
}

void add_poll_fd_sync(poll_array *pa, int fd) {
  assert(fd > 0);
  assert(fd < 16384);
  assert(pa != NULL);
  log_trace("Poller: Adding fd %d", fd);

  int next = pa->count < max_count ? pa->count : pa->last_in_index;
  assert(next < max_count);
  // Dont overwrite the listen_fd
  if (next == 0)
    next++;

  pa->fds[next].fd = fd;
  pa->fds[next].events = POLLIN; // Check ready-to-read
  pa->fds[next].revents = 0;

  if (pa->count < max_count)
    pa->count++;
  else {
    pa->last_in_index = (pa->last_in_index + 1) % max_count;
    // Dont overwrite the listen_fd
    if (pa->last_in_index == 0)
      pa->last_in_index++;
  }

  log_trace("Added new socket %d", fd);
  assert(pa->count <= max_count);
  assert(pa->count > 1);
  assert(pa->last_in_index < max_count);
  assert(pa->last_in_index > 0);
}

void remove_poll_fd_by_index_sync(poll_array *pa, int *i) {
  assert(*i > 0);
  assert(*i < max_count);
  assert(pa != NULL);
  assert(*i < pa->count);

  log_trace("Removing fd %d", pa->fds[*i].fd);

  close(pa->fds[*i].fd);

  for (int j = *i; j < pa->count - 1; j++) {
    pa->fds[j].fd = pa->fds[(j + 1)].fd;
    pa->fds[j].events = pa->fds[(j + 1)].events;
    pa->fds[j].revents = pa->fds[(j + 1)].revents;
  }
  pa->fds[pa->count - 1].fd = 0;
  pa->fds[pa->count - 1].events = 0;
  pa->fds[pa->count - 1].revents = 0;

  if (*i < pa->last_in_index)
    pa->last_in_index--;
  pa->count--;
  (*i)--;

  log_trace("Removed fd, new count is %d", pa->count);
  assert(pa->count > 0);
  assert(pa->last_in_index > 0);
}
