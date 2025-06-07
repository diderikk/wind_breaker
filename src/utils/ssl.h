#ifndef SSL_H
#define SSL_H

#include <openssl/ssl.h>

SSL_CTX *init_ssl_ctx();
void destroy_ssl_ctx();

#endif // SSL_H
