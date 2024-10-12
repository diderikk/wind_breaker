#include "signal.h"
#include "socket.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_PORT "8080"
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
  int socket_fd;
  struct addrinfo hints, *servinfo, *p;
  char data[BUFFER_SIZE];
  int rv;

  if (argc != 2) {
    perror("forgot client hostname\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(argv[1], SERVER_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((socket_fd = open_socket(p)) < 0)
      continue;

    if ((connect_socket(socket_fd, p->ai_addr, p->ai_addrlen)) < 0)
      continue;

    break;
  }

  if (p == NULL) {
    perror("Failed to connect\n");
    return 1;
  }

  printf("Connected to server at %s:%s\n", argv[1], SERVER_PORT);

  // Send message to server
  char *message = "Hello, server!";
  send_socket(socket_fd, message, strlen(message), SHOULD_EXIT);

  // Read response from server
  recv_socket(socket_fd, data, BUFFER_SIZE, SHOULD_EXIT);
  printf("Server response: %s\n", data);

  // Close the socket
  close(socket_fd);

  return 0;
}
