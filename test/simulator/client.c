#include "../../src/utils/logger.h"
#include "cases.h"
#include "signal.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_PORT "8080"
#define SERVER_IP "localhost"
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
  init_logger(TRACE, CONSOLE_ONLY, NULL);

  if (argc > 2) {
    printf("Running simulation against server at %s:%s\n", argv[1], argv[2]);
    run_cases(argv[1], argv[2]);
  } else if (argc > 1) {
    printf("Running simulation against server at %s:%s\n", argv[1],
           SERVER_PORT);
    run_cases(argv[1], SERVER_PORT);
  } else {
    printf("Running simulation against server at %s:%s\n", SERVER_IP,
           SERVER_PORT);
    run_cases(SERVER_IP, SERVER_PORT);
  }

  destroy_logger();

  return 0;
}
