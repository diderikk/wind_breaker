#ifndef SSL_ARRAY_H
#define SSL_ARRAY_H

#include <openssl/bio.h>
#include <openssl/ssl.h>

void init_ssl_array(SSL_CTX *ctx, int _max_size);
SSL *get_ssl_by_index(int index);
BIO *get_bio_by_index(int index);
void add_ssl_by_index(int index);
void remove_ssl_by_index(int index);
void destroy_ssl_array();

#endif // SSL_ARRAY_H
