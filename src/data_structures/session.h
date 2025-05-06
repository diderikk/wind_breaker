#ifndef SESSION_H
#define SESSION_H

#include "../static.h"
#include <openssl/ssl.h>

int init_session_cache(SSL_CTX *ctx);
void destroy_session_cache();

int get_session_id_for_thread();
void broadcast_session();
// Only works for INITIAL and SENT, since they can receive new data
void push_request(int related_fd, WORK_STATUS status);
struct session_full_return pop_request_by_fd(int related_fd);
struct session_full_return pop_request(WORK_STATUS status);

#endif // SESSION_H
