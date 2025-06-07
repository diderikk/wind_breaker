#ifndef POLL_ARRAY_H
#define POLL_ARRAY_H

#include "../static.h"

void init_poll_array(int listen_fd);
void init_poll_array_ssl(int listen_fd, int listen_fd_ssl);
void destroy_poll_array();

int add_poll_fd_sync(int fd);
void remove_poll_fd_by_index_sync(int *index);

#endif // POLL_ARRAY_H
