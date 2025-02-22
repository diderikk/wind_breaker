#ifndef LISTENER_H
#define LISTENER_H

int get_listener_socket(const char *port, int backlog);
// void listen_sync(int socket_fd); // Deprecated
// void listen_sync(int socket_fd, void *(*request_handler) (void *), void
// *request_handler_arg);
void listen_async(int socket_fd);
void *listener_worker_function(void *_arg);

#endif // LISTENER_H
