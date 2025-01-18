#include "signal2.h"
#include "../data_structures/worker_queue.h"
#include "../properties.h"
#include "../utils/logger.h"
#include "../worker.h"
#include "stop.h"
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

static int socket_fd1, socket_fd2;
static FILE *log_file;

static void handle_signal(int signum);
static char *get_signal_description(int signum);

void handle_signals(int socket_fd1, int socket_fd2, const FILE *log_file) {
  socket_fd1 = socket_fd1;
  socket_fd2 = socket_fd2;
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
}

void handle_signal(int signum) {
  log_info("Socket closed due to signal %s", get_signal_description(signum));
  stop_server();

  if (socket_fd1 != -1) {
    close(socket_fd1);
  }
  if (socket_fd2 != -1) {
    close(socket_fd2);
  }
  close_workers(get_worker_thread_max_size());

  if (log_file != NULL) {
    fclose(log_file);
  }
  exit(signum);
}

char *get_signal_description(int signum) {
  switch (signum) {
  case SIGINT:
    return "Interrupt from keyboard";
  case SIGTERM:
    return "Termination signal";
  case SIGQUIT:
    return "Quit from keyboard";
  default:
    return "Unknown signal";
  }
}
