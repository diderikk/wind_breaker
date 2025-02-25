#include "signal2.h"
#include "stop.h"
#include <stddef.h>

void handle_signals(void (*handle_exit)(int)) {
  handle_exit = handle_exit;
  struct sigaction sa;
  sa.sa_handler = handle_exit;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);

  signal(SIGPIPE, SIG_IGN);
}

char *get_signal_description(int signum) {
  switch (signum) {
  case SIGINT:
    return "Interrupt from keyboard";
  case SIGTERM:
    return "Termination signal";
  case SIGQUIT:
    return "Quit from keyboard";
  case SIGPIPE:
    return "Broken pipe";
  default:
    return "Unknown signal";
  }
}
