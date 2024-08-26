#ifndef LISTENER_H
#define LISTENER_H

typedef enum {
    RESET,
    REMOVE_FD,
    CONTINUE
} POLL_ERROR_CLASS;

#include "socket.h"
#include <poll.h>

int get_listener_socket(char* port);
void listen_sync(int socket_fd, size_t buffer_size);

#endif // LISTENER_H
