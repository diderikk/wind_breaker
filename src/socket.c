#include "socket.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <string.h>

static inline void disable_socket_blocking(int socket_fd);

int open_socket(const struct addrinfo *server_info) {
  int socket_fd;
  assert(server_info != NULL);
  assert(server_info->ai_family > 0);
  assert(server_info->ai_socktype > 0);
  assert(server_info->ai_protocol > 0);

  socket_fd = socket(server_info->ai_family, server_info->ai_socktype,
                     server_info->ai_protocol);

  assert(socket_fd != -1);
  return socket_fd;
}

int bind_socket(int socket_fd, const struct addrinfo *server_info) {
  int bind_result;
  assert(socket_fd > 0);
  assert(server_info != NULL);
  assert(server_info->ai_addr != NULL);
  assert(server_info->ai_addrlen > 0);

  bind_result = bind(socket_fd, (struct sockaddr *)server_info->ai_addr,
                     server_info->ai_addrlen);

  assert(bind_result != -1);
  return bind_result;
}

int listen_socket(int socket_fd, int backlog_size) {
  int listen_result;
  assert(socket_fd > 0);
  assert(backlog_size > 0);

  listen_result = listen(socket_fd, backlog_size);

  assert(listen_result != -1);
  return listen_result;
}

int accept_socket(int socket_fd, struct sockaddr *in_addr) {
  int client_socket_fd;
  socklen_t in_addr_size;
  assert(socket_fd > 0);
  assert(in_addr != NULL);

  in_addr_size = sizeof(*in_addr);
  client_socket_fd = accept(socket_fd, in_addr, &in_addr_size);
  disable_socket_blocking(client_socket_fd);

  assert(client_socket_fd != -1);
  log_info("Accepted socket %d", client_socket_fd);
  return client_socket_fd;
}

int connect_socket(int socket_fd, const struct sockaddr *in_addr,
                   size_t addr_length) {
  int connect_return;
  assert(socket_fd > 0);
  assert(in_addr != NULL);
  assert(addr_length > 0);

  connect_return = connect(socket_fd, in_addr, addr_length);

  assert(connect_return != -1);
  return connect_return;
}

int send_ssl(SSL *ssl, const char *buffer, size_t buffer_size) {
  int send_return;
  assert(ssl != NULL);
  assert(buffer != NULL);
  assert(buffer_size > 0);

  send_return = SSL_write(ssl, buffer, buffer_size);

  assert(send_return != -2);
  if (send_return == -1) {
    log_error("SSL write error: %d %s", SSL_get_error(ssl, send_return),
              ERR_reason_error_string(SSL_get_error(ssl, send_return)));
  }
  return send_return;
}

int send_bio(BIO *bio, const char *buffer, size_t buffer_size) {
  int send_return;
  assert(bio != NULL);
  assert(buffer != NULL);
  assert(buffer_size > 0);

  send_return = BIO_write(bio, buffer, buffer_size);

  assert(send_return != -2);
  if (send_return == -1) {
    log_error("BIO write error");
  }
  return send_return;
}

// TODO: Deprecated
int send_socket(int socket_fd, const char *buffer, size_t buffer_size) {
  int send_return;
  assert(socket_fd > 0);
  assert(buffer != NULL);
  assert(buffer_size > 0);

  send_return = send(socket_fd, buffer, buffer_size, 0);

  if (send_return == -1) {
    log_error("send error");
    return -1;
  }
  return send_return;
}

int recv_ssl(SSL *ssl, char *buffer, size_t buffer_size) {
  int recv_return;
  assert(ssl != NULL);
  assert(buffer != NULL);
  assert(buffer_size > 0);

  memset(buffer, 0, buffer_size);
  recv_return = SSL_read(ssl, buffer, buffer_size - 1);

  if (recv_return < 0) {
    log_error("SSL read error");
    return -1;
  }

  buffer[buffer_size] = '\0';
  return recv_return;
}

int recv_bio(BIO *bio, char *buffer, size_t buffer_size) {
  int recv_return;
  assert(bio != NULL);
  assert(buffer != NULL);
  assert(buffer_size > 0);

  memset(buffer, 0, buffer_size);
  recv_return = BIO_read(bio, buffer, buffer_size - 1);

  if (recv_return < 0) {
    log_error("BIO read error");
    return -1;
  }

  buffer[buffer_size] = '\0';
  return recv_return;
}

// TODO: Deprecated
int recv_socket(int socket_fd, char *buffer, size_t buffer_size) {
  int recv_return;
  assert(socket_fd > 0);
  assert(buffer != NULL);
  assert(buffer_size > 0);

  memset(buffer, 0, buffer_size);
  recv_return = recv(socket_fd, buffer, buffer_size - 1, 0);

  if (recv_return == -1) {
    log_error("recv error");
    return -1;
  }

  buffer[buffer_size] = '\0';
  return recv_return;
}

// TODO: Move to separate file
// https://beej.us/guide/bgnet/html/split/client-server-background.html
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(const struct sockaddr *sa) {
  assert(sa != NULL);

  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int get_in_addr_port(const struct sockaddr *sa) {
  assert(sa != NULL);

  if (sa->sa_family == AF_INET) {
    return ntohs(((struct sockaddr_in *)sa)->sin_port);
  }

  return ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
}

void get_in_addr_str(const struct sockaddr *sa, char *buffer, size_t length) {
  assert(sa != NULL);

  inet_ntop(sa->sa_family, get_in_addr(sa), buffer, length);
}

// https://stackoverflow.com/questions/1543466/how-do-i-change-a-tcp-socket-to-be-non-blocking
static inline void disable_socket_blocking(int socket_fd) {
  int flags;
  assert(socket_fd > 0);

  flags = fcntl(socket_fd, F_GETFL, 0);
  assert(flags != -1);

  flags |= O_NONBLOCK;
  assert(fcntl(socket_fd, F_SETFL, flags) != -1);
  log_trace("Set socket %d to non-blocking", socket_fd);
}
