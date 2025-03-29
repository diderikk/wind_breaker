#ifndef LISTENER_H
#define LISTENER_H

#include "static.h"

int get_listener_socket(const char *port, int backlog);
// void listen_sync(int socket_fd); // Deprecated
// void listen_sync(int socket_fd, void *(*request_handler) (void *), void
// *request_handler_arg);
void listen_async(int socket_fd);
void *listener_worker_function(void *_arg);
void _listen(int listener, int (*new_connection_handler)(SSL *, BIO *),
             void (*close_connection_handler)(int),
             int (*request_handler)(int, char *));
int handle_request_async(int fd, char *buffer);

#endif // LISTENER_H
