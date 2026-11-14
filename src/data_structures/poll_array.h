#ifndef POLL_ARRAY_H
#define POLL_ARRAY_H

void init_poll_array(int listen_fd);
void init_poll_array_ssl(int listen_fd, int listen_fd_ssl);
void destroy_poll_array();

struct pollfd *get_poll_fd_array(void);
int get_poll_fd_array_count(void);
int add_poll_fd_sync(int fd);
void remove_poll_fd_by_index_sync(int index);

#endif // POLL_ARRAY_H
