#include "signal2.h"
#include "logger.h"
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t server_stop = 0;
static int socket_fd1, socket_fd2;
static FILE *log_file;

static void handle_signal(int signum);
sig_atomic_t stop() { return server_stop; }

void handle_signals(int socket_fd1, int socket_fd2, FILE *log_file) {
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
  server_stop = 1;

  log_info("Socket closed due to signal %d", signum);
  if (log_file != NULL) {
    fclose(log_file);
  }
  if (socket_fd1 != -1) {
    close(socket_fd1);
  }
  if (socket_fd2 != -1) {
    close(socket_fd2);
  }
  // close_workers(get_worker_thread_max_size());
  exit(signum);
}
