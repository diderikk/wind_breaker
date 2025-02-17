#ifndef SESSION_H
#define SESSION_H

#include <openssl/bio.h>
#include <openssl/ssl.h>

struct session {
  int id;
  int related_fd;
  unsigned long thread_id;
};

struct session_full_return {
  struct session *session;
  BIO *bio;
  SSL *ssl;
};

int init_session_cache(int _max_size, SSL_CTX *ctx);
void destroy_session_cache();
struct session_full_return add_thread_to_session(int related_fd);
void remove_thread_from_session();
struct session_full_return add_to_session_sync(int related_fd);
struct session_full_return get_session_for_thread();
void del_from_session_sync(int related_fd);

#endif // SESSION_H
