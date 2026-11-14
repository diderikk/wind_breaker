#ifndef CONNECTION_CASES_H
#define CONNECTION_CASES_H

#include "../../src/data_structures/buffer.h"

struct connection_data {
  const char *ip;
  const char *port;
  const long seed;
};

int test_connection(char *ip, char *port);
void *start_connection_delay_before_close(void *arg);
void *start_connections_simultaneously(void *arg);
void *start_connection_send_recv_ten_times(void *arg);
void *start_connection_close(void *arg);
int connect_to_server(const char *ip, const char *port);
// Reads from the server into `response` with recv_bio. Returns once the server
// closes the connection or nothing more arrives within `timeout_ms`.
// Returns the number of bytes in `response`, 0 if nothing was received.
int recv_response(int socket_fd, buffer *response, int timeout_ms);

#endif // CONNECTION_CASES_H
