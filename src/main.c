#include "listener.h"
#include "signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils/logger.h"

#define PORT "8080"

int socket_fd = -1;

void handle_signal(int signum);

int main(int argc, char *argv[]) {
  log_info("Running main with %d args:", argc);
  for (int i = 0; i < argc; ++i) {
    log_debug("Argument %d: %s", i + 1, argv[i]);
  }

  socket_fd = get_listener_socket(PORT);

  if (socket_fd < 0) {
    log_error("Failed to initalize socket");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  listen_async(socket_fd);

  return 0;
}

void handle_signal(int signum) {
  if (socket_fd != -1) {
    close(socket_fd);
    log_info("\nSocket closed due to signal %d", signum);
  }
  exit(signum);
}
