#ifndef LISTENER_SSL_H
#define LISTENER_SSL_H

#include "static.h"

SSL_CTX *init_ssl_ctx();
void destroy_ssl_ctx();
void *listen_async_ssl(int *socket_fd);

#endif // LISTENER_SSL_H
