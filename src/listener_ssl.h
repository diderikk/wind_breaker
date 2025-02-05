#ifndef LISTENER_SSL_H
#define LISTENER_SSL_H

void listen_async_ssl(int socket_fd);
void destroy_ssl_listener();

#endif // LISTENER_SSL_H
