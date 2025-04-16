#ifndef LISTENER_H
#define LISTENER_H

#include "static.h"

int get_listener_socket(const char *port, int backlog);
void listen_async(int socket_fd);
void _listen(int listener, int (*new_connection_handler)(),
             void (*close_connection_handler)(int),
             int (*request_handler)(int));
int handle_request_async(int fd);

#endif // LISTENER_H
