
#ifndef LISTENER_H
#define LISTENER_H

#include "socket.h"
#include <poll.h>

int get_listener_socket(char* port);
void listen_sync(int socket_fd, size_t buffer_size);

#endif // LISTENER_H
