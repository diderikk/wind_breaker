#include "data_structures/session.h"
#include "data_structures/worker_queue.h"
#include "listener.h"
#include "listener_ssl.h"
#include "properties.h"
#include "shutdown/signal2.h"
#include "shutdown/stop.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include "worker.h"
#include <unistd.h>

static int http_socket_fd, https_socket_fd;
static FILE *log_file;
void handle_exit(int signum) {
  log_info("Socket closed due to signal %s", get_signal_description(signum));
  stop_server();
  if (http_socket_fd != -1) {
    close(http_socket_fd);
  }
  if (https_socket_fd != -1) {
    close(https_socket_fd);
  }
  destroy_session_cache();
  close_workers(get_worker_thread_max_size());

  if (log_file != NULL) {
    fclose(log_file);
  }
  destroy_ssl_listener();
  exit(signum);
}

int main(int argc, char *argv[]) {

  log_logo();
  init_properties(argc, argv);
  log_file = logger_init(get_log_level(), get_log_type(), get_log_file());
  // Log file is opened if LOG_TYPE is FILE_ONLY or CONSOLE_FILE
  assert(get_log_type() == CONSOLE_ONLY || log_file != NULL);

  if (strcasecmp(get_env(), "dev") == 0) {
    log_debug("PID: %d", getpid());
  }

  http_socket_fd =
      get_listener_socket(get_http_port(), get_listen_backlog_max_size());
  assert(http_socket_fd >= 0);

  https_socket_fd =
      get_listener_socket(get_https_port(), get_listen_backlog_max_size());

  if (https_socket_fd >= 0)
    init_session_cache(get_session_max_size(), NULL);
  else
    init_session_cache(get_session_max_size(), NULL);

  assert(init_queue() == 0);
  assert(init_workers(get_worker_thread_max_size(), listener_worker_function,
                      NULL) == 0);

  handle_signals(handle_exit);

  listen_async_ssl(https_socket_fd);

  listen_async(http_socket_fd);

  return 0;
}
