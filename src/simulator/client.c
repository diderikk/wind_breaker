#include "../socket.h"
#include "../utils/logger.h"
#include "cases.h"
#include "signal.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_PORT "8080"
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {

  if (argc > 2) {
    log_info("Running simulation against server at %s:%s", argv[1], argv[2]);
    run_cases(argv[1], argv[2]);
  } else if (argc > 1) {
    log_info("Running simulation against server at %s:%s", argv[1],
             SERVER_PORT);
    run_cases(argv[1], SERVER_PORT);
  } else {
    log_info("Running simulation against server at %s:%s", SERVER_IP,
             SERVER_PORT);
    run_cases(SERVER_IP, SERVER_PORT);
  }

  return 0;
}
