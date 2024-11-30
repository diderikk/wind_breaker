#ifndef POLL_ARRAY_H
#define POLL_ARRAY_H

#include <poll.h>

void init_poll_array(int listen_fd);
struct pollfd *get_poll_array();
struct pollfd *get_poll_fd_by_index(int index);
int get_poll_array_size();
void add_poll_fd_sync(int fd);
void remove_poll_fd_by_index_sync(int *index);

#endif // POLL_ARRAY_H
