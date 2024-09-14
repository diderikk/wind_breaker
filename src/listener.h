#ifndef LISTENER_H
#define LISTENER_H
#include "http/static.h"
#define LISTEN_BACKLOG 50 // TODO: Justify
#define BUFFER_SIZE 1024

#include "http/request.h"
#include "http/response.h"
#include "socket.h"
#include "worker.h"
#include "worker_queue.h"
#include <poll.h>

typedef enum { RESET, REMOVE_FD, CONTINUE } POLL_ERROR_CLASS;

typedef struct {
  int fd;
  char data[HTTP_BODY_SIZE];
} request_data;

int get_listener_socket(char *port);
void listen_sync(int socket_fd);
// void listen_sync(int socket_fd, void *(*request_handler) (void *), void
// *request_handler_arg);
void listen_async(int socket_fd);

#endif // LISTENER_H
