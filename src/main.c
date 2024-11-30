#include "listener.h"
#include "signal.h"
#include "utils/assert2.h"
#include "utils/logger.h"

#define PORT "8080"
#define LOG_FILE_PATH "/home/diderikk/Dokumenter/Projects/CServer/app.log"
#define LOG_LEVEL TRACE
#define LOG_TYPE CONSOLE_FILE

static int socket_fd = -1;
static FILE *log_file;

void handle_signal(int signum);

int main(int argc, char *argv[]) {
  log_file = logger_init(LOG_LEVEL, LOG_TYPE, LOG_FILE_PATH);
  // Log file is opened if LOG_TYPE is FILE_ONLY or CONSOLE_FILE
  assert(LOG_TYPE == CONSOLE_ONLY || log_file != NULL);

  log_info("Running main with %d args:", argc);
  for (int i = 0; i < argc; ++i) {
    log_debug("Argument %d: %s", i + 1, argv[i]);
  }

  socket_fd = get_listener_socket(PORT);
  assert(socket_fd >= 0);

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

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
  exit(signum);
}
