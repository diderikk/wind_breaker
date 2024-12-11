#ifndef CONNECTION_CASES_H
#define CONNECTION_CASES_H

struct connection_data {
  const char *ip;
  const char *port;
  const long seed;
};

int test_connection(char *ip, char *port);
void *start_connection_delay_before_close(void *arg);
void *start_connections_simultaneously(void *arg);
int connection_send_random_times(void *arg);
int connect_to_server(const char *ip, const char *port);

#endif // CONNECTION_CASES_H
