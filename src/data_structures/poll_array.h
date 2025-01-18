#ifndef POLL_ARRAY_H
#define POLL_ARRAY_H

#include <poll.h>

typedef struct {
  struct pollfd *fds;
  int count, last_in_index;
} poll_array;

void init_poll_array(poll_array **pa, int listen_fd);
void destroy_poll_array(poll_array *pa);
void add_poll_fd_sync(poll_array *pa, int fd);
void remove_poll_fd_by_index_sync(poll_array *pa, int *index);

#endif // POLL_ARRAY_H
