#ifndef LISTENER_H
#define LISTENER_H

#include "static.h"

int get_listener_socket(const char *port, int backlog);
void listen_async(int socket_fd, int socket_fd_ssl);
int handle_request_async(int fd);

#endif // LISTENER_H
