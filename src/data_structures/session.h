#ifndef SESSION_H
#define SESSION_H

#include <openssl/ssl.h>

int init_session_cache(int _max_size, SSL_CTX *ctx);
void destroy_session_cache();

struct session_full_return add_to_session_sync(int related_fd);
void del_from_session_sync(int related_fd);
struct session_full_return get_session_sync(int related_fd);

struct session_full_return add_thread_to_session(int related_fd);
void remove_thread_from_session();
struct session_full_return get_session_for_thread();

#endif // SESSION_H
