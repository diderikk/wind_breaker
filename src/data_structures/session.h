#ifndef SESSION_H
#define SESSION_H

#include "../static.h"
#include <openssl/ssl.h>

int init_session_cache(SSL_CTX *ctx);
void destroy_session_cache();

void broadcast_session();
// Only works for INITIAL and SENT, since they can receive new data
void push_request(int related_fd, WORK_STATUS status);
session_t *pop_request_by_fd(int related_fd);
session_t *pop_request(WORK_STATUS status);

#endif // SESSION_H
