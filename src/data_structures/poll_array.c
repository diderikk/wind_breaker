#include "poll_array.h"
#include "../properties.h"
#include "../static.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include "marked_fds.h"
#include <unistd.h>

static int count;
static int capacity;
static struct pollfd *fds;
static int listener_count;

// Initialize the poll array with the listen_fd
// This should be called once, when the server fronts
void init_poll_array(int listen_fd) {
  assert(listen_fd > 0);
  assert(listen_fd < MAX_FD);
  assert(fds == NULL);

  capacity = SESSION_DEFAULT_SIZE;

  fds = calloc(capacity, sizeof(struct pollfd));
  assert(fds != NULL);

  log_info("Initialized poll array with count %d", capacity);
  fds[0].fd = listen_fd;
  fds[0].events = POLLIN; // Report ready to read on incoming connection
  count = 1;
  listener_count = 1;
}

void init_poll_array_ssl(int listen_fd, int listen_fd_ssl) {
  init_poll_array(listen_fd);
  assert(listen_fd_ssl > 0);
  assert(listen_fd_ssl < MAX_FD);
  assert(listen_fd != listen_fd_ssl);
  assert(get_enabled_ssl() == 1);

  fds[1].fd = listen_fd_ssl;
  fds[1].events = POLLIN; // Report ready to read on incoming connection
  count = 2;
  listener_count = 2;
}

void destroy_poll_array() {
  free(fds);
  fds = NULL;
  assert(fds == NULL);

  count = 0;
  capacity = 0;
  listener_count = 0;
}

struct pollfd *get_poll_fd_array() { return fds; }
int get_poll_fd_array_count() { return count; }

int add_poll_fd_sync(int fd) {
  assert(fd > 0);
  assert(fd < MAX_FD);
  assert(fds != NULL);
  int max_count = get_session_max_size() + listener_count;

  if (count + 1 > max_count)
    return -1;

  if (count + 1 > capacity) {
    int new_capacity;
    if (capacity * 2 > max_count) {
      new_capacity = max_count;
    } else {
      new_capacity = capacity * 2;
    }

    struct pollfd *temp_fds =
        realloc(fds, sizeof(struct pollfd) * new_capacity);
    if (temp_fds == NULL) {
      log_error("poll_array failed to increase to capacity: %d", new_capacity);
      return -1;
    }

    fds = temp_fds;
    capacity = new_capacity;
  }

  fds[count].fd = fd;
  fds[count].events = POLLIN; // Check ready-to-read
  fds[count].revents = 0;

  count++;

  log_trace("Added poll_fd %d", fd);
  assert(count <= capacity);
  assert(count > 1);
  return count - 1;
}

void remove_poll_fd_by_index_sync(int i) {
  assert(i >= listener_count);
  assert(i < capacity);
  assert(fds != NULL);
  assert(i < count);

  close(fds[i].fd);
  unmark(fds[i].fd);

  fds[i].fd = fds[count - 1].fd;
  fds[i].events = fds[count - 1].events;
  fds[i].revents = fds[count - 1].revents;

  fds[count - 1].fd = 0;
  fds[count - 1].events = 0;
  fds[count - 1].revents = 0;

  count--;

  log_trace("Removed poll_fd, new count is %d", count);
  assert(count > 0);
}
