#ifndef SESSION_H
#define SESSION_H

#include "../static.h"
#include <openssl/ssl.h>

int init_session_cache(SSL_CTX *ctx);
void destroy_session_cache();

void broadcast_session();
// Only works for INITIAL and SENT, since they can receive new data
void push_session(int related_fd, WORK_STATUS status);
session_t *pop_session_for_read(int related_fd);
session_t *pop_session(WORK_STATUS status);

#endif // SESSION_H
