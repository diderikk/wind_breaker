#include "socket.h"
#include "string.h"
#include "utils/logger.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

void error(int socket_fd, EXIT_ACTION should_exit);
const char *handle_socket_error(int code);
const char *handle_bind_error(int code);
const char *handle_listen_error(int code);
const char *handle_accept_error(int code);
const char *handle_connect_error(int code);
const char *handle_send_error(int code);
const char *handle_recv_error(int code);

int open_socket(struct addrinfo *server_info) {
  int socket_fd;

  socket_fd = socket(server_info->ai_family, server_info->ai_socktype,
                     server_info->ai_protocol);

  if (socket_fd == -1) {
    error(socket_fd, SHOULD_NOT_EXIT);
  }

  return socket_fd;
}

int bind_socket(int socket_fd, struct addrinfo *server_info) {
  int bind_result;

  bind_result = bind(socket_fd, (struct sockaddr *)server_info->ai_addr,
                     server_info->ai_addrlen);

  if (bind_result < 0) {
    error(socket_fd, SHOULD_NOT_EXIT);
  }

  return bind_result;
}

int listen_socket(int socket_fd, int backlog_size) {
  int listen_result;

  listen_result = listen(socket_fd, backlog_size);

  if (listen_result < 0) {
    error(socket_fd, SHOULD_EXIT);
  }

  return listen_result;
}

int accept_socket(int socket_fd, struct sockaddr *in_addr) {
  int client_socket_fd;
  socklen_t in_addr_size;

  in_addr_size = sizeof(*in_addr);
  client_socket_fd = accept(socket_fd, in_addr, &in_addr_size);

  if (client_socket_fd < 0) {
    error(socket_fd, SHOULD_NOT_EXIT);
  }

  log_info(__FILE__, "Accepted socket %d", client_socket_fd);

  return client_socket_fd;
}

int connect_socket(int socket_fd, struct sockaddr *in_addr,
                   size_t addr_length) {
  int connect_return;

  connect_return = connect(socket_fd, in_addr, addr_length);

  if (connect_return < 0) {
    error(socket_fd, SHOULD_NOT_EXIT);
  }

  return connect_return;
}

int send_socket(int socket_fd, char *buffer, size_t buffer_size,
                EXIT_ACTION should_exit) {
  int send_return;

  send_return = send(socket_fd, buffer, buffer_size, 0);
  if (send_return < 0) {
    error(socket_fd, should_exit);
  }

  return send_return;
}

int recv_socket(int socket_fd, char *buffer, size_t buffer_size,
                EXIT_ACTION should_exit) {
  int recv_return;

  memset(buffer, 0, buffer_size);
  recv_return = recv(socket_fd, buffer, buffer_size - 1, 0);
  if (recv_return < 0) {
    error(socket_fd, should_exit);
  }
  buffer[buffer_size] = '\0';

  return recv_return;
}

// TODO: Move to separate file
// https://beej.us/guide/bgnet/html/split/client-server-background.html
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int get_in_addr_port(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return ntohs(((struct sockaddr_in *)sa)->sin_port);
  }

  return ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
}

void get_in_addr_str(struct sockaddr *sa, char *buffer, size_t length) {
  inet_ntop(sa->sa_family, get_in_addr(sa), buffer, length);
}

void error(int socket_fd, EXIT_ACTION should_exit) {
  log_error(__FILE__, "Error: %s", strerror(errno));

  if (socket_fd != -1)
    close(socket_fd);
  if (should_exit == SHOULD_EXIT)
    exit(EXIT_FAILURE);
}
