#include "cases.h"
#include "../../src/utils/assert2.h"
#include "connection_cases.h"
#include "curl_cases.c"
#include "http_cases.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GRN "\x1B[32m"
#define RESET "\x1B[0m"

static int case_count = 0;
static char *case_names[100];
static pthread_t *cases = NULL;

int start_case(void *(*func)(void *), struct connection_data *arg,
               const char *name);

void run_cases(char *ip, char *port) {
  long seed = time(NULL);
  printf("Running cases with seed %ld\n", seed);
  cases = (pthread_t *)malloc(sizeof(pthread_t) * 100);
  // Validate server is reachable
  assert(test_connection(ip, port) == 0);

  struct connection_data arg = {ip, port, seed};
  start_case(start_connection_delay_before_close, &arg,
             "start_connection_delay_before_close");
  start_case(start_connection_send_recv_ten_times, &arg,
             "start_connection_send_recv_ten_times");
  start_case(start_connection_close, &arg, "start_connection_close");
  sleep(1);
  start_case(start_connections_simultaneously, &arg,
             "start_connections_simultaneously");
  sleep(1);
  start_case(start_http_get_request, &arg, "start_http_get_request");
  start_case(start_http_get_request_gzip, &arg, "start_http_get_request_gzip");
  start_case(start_http_get_request_deflate, &arg,
             "start_http_get_request_deflate");
  start_case(start_http_get_request_not_found, &arg,
             "start_http_get_request_not_found");
  sleep(1);
  start_case(start_curl_get_request, &arg, "start_curl_get_request");
  start_case(start_curl_get_all_posts, &arg, "start_curl_get_all_posts");
  start_case(start_curl_get_all_projects, &arg, "start_curl_get_all_projects");

  for (int i = 0; i < case_count; i++) {
    pthread_join(cases[i], NULL);
    printf(GRN "Case: %s passed\n" RESET, case_names[i]);
  }
  free(cases);
  cases = NULL;

  printf("\nCompleted %d/%d simulator tests\n", case_count, case_count);
}

int start_case(void *(*func)(void *), struct connection_data *arg,
               const char *name) {
  assert(cases != NULL);
  printf("Starting case %d, named: %s\n", case_count, name);

  assert(pthread_create(&cases[case_count++], NULL, func, arg) == 0);
  case_names[case_count - 1] = (char *)malloc(strlen(name) + 1);
  strcpy(case_names[case_count - 1], name);

  return 0;
}
