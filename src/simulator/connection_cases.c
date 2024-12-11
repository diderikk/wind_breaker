#include "connection_cases.h"
#include "../socket.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void *start_close_connection(void *arg);
void sleep_ms(int ms);

int test_connection(char *ip, char *port) {
  int socket_fd = connect_to_server(ip, port);
  if (socket_fd < 0) {
    log_error("Failed to connect to server at %s:%s", ip, port);
    return -1;
  }
  log_info("Connection test successful");
  close(socket_fd);
  return 0;
}

void *start_connection_delay_before_close(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;

  int socket_fd = connect_to_server(data->ip, data->port);
  assert(socket_fd > 0);

  int err;
  socklen_t len = sizeof(err);
  int sleep_time = data->seed % 301;

  log_trace("Sleeping for %d seconds", sleep_time);
  // Sleep for a random amount of time. Max 300 seconds
  sleep(sleep_time);
  assert(getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &err, &len) != -1);

  close(socket_fd);
  return 0;
}

void *start_connections_simultaneously(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;

  int amount_of_connections = data->seed % 30;
  pthread_t *threads =
      (pthread_t *)malloc(sizeof(pthread_t) * amount_of_connections);
  assert(threads != NULL);

  log_info("Starting %d connections simultaneously", amount_of_connections);
  for (int i = 0; i < amount_of_connections; i++) {
    assert(pthread_create(&threads[i], NULL, start_close_connection, data) ==
           0);
    sleep_ms(100);
  }

  for (int i = 0; i < amount_of_connections; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}

void *start_close_connection(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;

  int socket_fd = connect_to_server(data->ip, data->port);
  assert(socket_fd > 0);

  sleep(1);

  log_trace("Connection established. Closing connection immediately");
  close(socket_fd);
  return 0;
}

void sleep_ms(int ms) {
  struct timespec req;
  req.tv_sec = 0;
  req.tv_nsec = ms * 1000000L;
  nanosleep(&req, NULL);
}

int connect_to_server(const char *ip, const char *port) {
  int socket_fd;
  struct addrinfo hints, *servinfo, *p;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
    log_error("getaddrinfo: %s", gai_strerror(rv));
    return -1;
  }
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((socket_fd = open_socket(p)) < 0)
      continue;

    if ((connect_socket(socket_fd, p->ai_addr, p->ai_addrlen)) < 0)
      continue;

    break;
  }

  if (p == NULL) {
    log_error("Failed to connect");
    return -1;
  }

  log_trace("Connected to server at %s:%s", ip, port);

  // Send message to server
  // char *message = "Hello, server!";
  // send_socket(socket_fd, message, strlen(message));

  // Read response from server
  // recv_socket(socket_fd, data, BUFFER_SIZE);
  // printf("Server response: %s\n", data);

  // Close the socket
  freeaddrinfo(servinfo);
  return socket_fd;
}
