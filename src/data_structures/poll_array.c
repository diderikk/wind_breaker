#include "poll_array.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include <stdlib.h>
#include <unistd.h>

#define POLL_ARRAY_SIZE 64

static struct pollfd *poll_array = NULL;
static int poll_array_size = 0;

// Initialize the poll array with the listen_fd
// This should be called once, when the server starts
void init_poll_array(int listen_fd) {
  assert(listen_fd > 0);
  assert(poll_array == NULL);

  poll_array = malloc(sizeof *poll_array * POLL_ARRAY_SIZE);
  for (int i = 0; i < POLL_ARRAY_SIZE; i++) {
    poll_array[i].fd = 0;
    poll_array[i].events = 0;
    poll_array[i].revents = 0;
  }

  log_info("Initialized poll array with size %d", POLL_ARRAY_SIZE);
  poll_array[0].fd = listen_fd;
  poll_array[0].events = POLLIN; // Report ready to read on incoming connection
  poll_array_size = 1;
}

struct pollfd *get_poll_array() {
  assert(poll_array != NULL);
  return poll_array;
}

struct pollfd *get_poll_fd_by_index(int index) {
  assert(index >= 0);
  assert(index < poll_array_size);
  assert(poll_array != NULL);

  return &poll_array[index];
}

int get_poll_array_size() {
  assert(poll_array_size > 0);
  return poll_array_size;
}

void add_poll_fd_sync(int fd) {
  assert(fd > 0);
  assert(poll_array != NULL);
  log_trace("Poller: Adding fd %d", fd);

  // If we don't have room, reset the array size
  if (poll_array_size == POLL_ARRAY_SIZE) {
    poll_array_size = 1;
  }

  poll_array[poll_array_size].fd = fd;
  poll_array[poll_array_size].events = POLLIN; // Check ready-to-read

  poll_array_size++;

  assert(poll_array_size <= POLL_ARRAY_SIZE);
  assert(poll_array_size > 0);
  log_trace("Added new socket %d", fd);
}

void remove_poll_fd_by_index_sync(int *i) {
  assert(*i >= 0);
  assert(*i < poll_array_size);
  assert(poll_array != NULL);

  log_trace("Removing fd %d", poll_array[*i].fd);

  close(poll_array[*i].fd);
  // Copy the one from the end over this one
  for (int j = *i; j < poll_array_size - 1; j++) {
    poll_array[j] = poll_array[j + 1];
  }

  poll_array_size--;
  (*i)--;

  assert(poll_array_size > 0);
}
