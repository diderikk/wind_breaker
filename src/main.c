#include "data_structures/marked_fds.h"
#include "data_structures/poll_array.h"
#include "data_structures/session.h"
#include "data_structures/session_by_thread_map.h"
#include "listener.h"
#include "properties.h"
#include "shutdown/signal2.h"
#include "shutdown/stop.h"
#include "utils/assert2.h"
#include "utils/db_migration.h"
#include "utils/logger.h"
#include "utils/ssl.h"
#include "worker.h"
#include <unistd.h>

static int http_socket_fd = -1, https_socket_fd = -1;
static FILE *log_file;
static SSL_CTX *ctx;
static int status = 0;

void handle_exit(int signum) {
  log_info("Socket closed due to signal %s", get_signal_description(signum));
  stop_server();
  if (http_socket_fd != -1 && status >= 5) {
    close(http_socket_fd);
  }
  if (https_socket_fd != -1 && status >= 6) {
    close(https_socket_fd);
  }
  if (status >= 10)
    close_workers(get_worker_thread_max_size());

  if (status >= 9)
    destroy_poll_array();
  if (status >= 8)
    destroy_session_cache();
  if (status >= 7)
    destroy_marked_fds();
  if (ctx != NULL) {
    destroy_ssl_ctx();
    ctx = NULL;
  }
  if (status >= 2)
    destroy_logger();
  destroy_session_by_thread_map();
  exit(signum);
}

int main(int argc, char *argv[]) {
  handle_signals(handle_exit);
  init_properties(argc, argv);
  status = 1;
  log_file = init_logger(get_log_level(), get_log_type(), get_log_file());
  status = 2;
  // Log file is opened if LOG_TYPE is FILE_ONLY or CONSOLE_FILE
  assert(get_log_type() == CONSOLE_ONLY || log_file != NULL);
  status = 3;

  log_debug("PID: %d", getpid());

  assert(migrate_db() == 0);
  status = 4;

  http_socket_fd =
      get_listener_socket(get_http_port(), get_listen_backlog_max_size());
  assert(http_socket_fd >= 0);
  status = 5;

  if (get_enabled_ssl() == 1) {
    https_socket_fd =
        get_listener_socket(get_https_port(), get_listen_backlog_max_size());
    status = 6;
    if (https_socket_fd >= 0)
      ctx = init_ssl_ctx();
  }

  init_marked_fds();
  status = 7;
  init_session_cache(ctx);
  status = 8;
  if (get_enabled_ssl() == 1) {
    init_poll_array_ssl(http_socket_fd, https_socket_fd);
  } else {
    init_poll_array(http_socket_fd);
  }
  status = 9;
  assert(init_workers(get_worker_thread_max_size(), NULL) == 0);
  status = 10;

  listen_async(http_socket_fd, https_socket_fd);

  return 0;
}
