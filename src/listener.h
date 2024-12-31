#ifndef LISTENER_H
#define LISTENER_H
#define LISTEN_BACKLOG 50 // TODO: Justify

#include "data_structures/worker_queue.h"
#include "http/request.h"
#include "http/response.h"
#include "socket.h"
#include "static.h"
#include "worker.h"
#include <poll.h>

typedef enum { RESET, REMOVE_FD, CONTINUE } POLL_ERROR_CLASS;

int get_listener_socket(const char *port);
// void listen_sync(int socket_fd); // Deprecated
// void listen_sync(int socket_fd, void *(*request_handler) (void *), void
// *request_handler_arg);
void listen_async(int socket_fd);

#endif // LISTENER_H
