#include "data_structures/session.h"
#include "listener.h"
#include "properties.h"
#include "signal.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include "worker.h"

static int socket_fd = -1;
static FILE *log_file;

void handle_signal(int signum);

int main(int argc, char *argv[]) {

  init_properties(argc, argv);
  init_session_cache(get_session_max_size());
  log_logo();
  log_file = logger_init(get_log_level(), get_log_type(), get_log_file());
  // Log file is opened if LOG_TYPE is FILE_ONLY or CONSOLE_FILE
  assert(get_log_type() == CONSOLE_ONLY || log_file != NULL);

  if (strcasecmp(get_env(), "dev") == 0) {
    log_debug("PID: %d", getpid());
  }

  socket_fd =
      get_listener_socket(get_http_port(), get_listen_backlog_max_size());
  assert(socket_fd >= 0);

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  assert(init_queue() == 0);
  assert(init_workers(get_worker_thread_max_size(), listener_worker_function,
                      NULL) == 0);

  listen_async(socket_fd);

  return 0;
}

void handle_signal(int signum) {
  log_info("Socket closed due to signal %d", signum);
  if (log_file != NULL) {
    fclose(log_file);
  }
  if (socket_fd != -1) {
    close(socket_fd);
  }
  // close_workers(get_worker_thread_max_size());
  exit(signum);
}
