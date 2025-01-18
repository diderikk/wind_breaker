#include "data_structures/session.h"
#include "listener.h"
#include "properties.h"
#include "shutdown/signal2.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include "worker.h"

void handle_signal(int signum);

int main(int argc, char *argv[]) {

  log_logo();
  init_properties(argc, argv);
  init_session_cache(get_session_max_size());
  FILE *log_file = logger_init(get_log_level(), get_log_type(), get_log_file());
  // Log file is opened if LOG_TYPE is FILE_ONLY or CONSOLE_FILE
  assert(get_log_type() == CONSOLE_ONLY || log_file != NULL);

  if (strcasecmp(get_env(), "dev") == 0) {
    log_debug("PID: %d", getpid());
  }

  int http_socket_fd =
      get_listener_socket(get_http_port(), get_listen_backlog_max_size());
  assert(http_socket_fd >= 0);

  int https_socket_fd =
      get_listener_socket(get_https_port(), get_listen_backlog_max_size());

  assert(init_queue() == 0);
  assert(init_workers(get_worker_thread_max_size(), listener_worker_function,
                      NULL) == 0);

  handle_signals(http_socket_fd, https_socket_fd, log_file);

  listen_async(http_socket_fd);

  return 0;
}
