#include "properties.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

// https://github.com/openssl/openssl/blob/master/demos/guide/tls-server-block.c

static SSL_CTX *ctx = NULL;
static const unsigned char cache_id[] = "wind_breaker_server";
static SSL **ssl_array = NULL;
static BIO **bio_array = NULL;

void init_ssl_listener() {
  long opts;
  ctx = SSL_CTX_new(TLS_server_method());
  assert(ctx != NULL);

  if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
    SSL_CTX_free(ctx);
    assert(!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION));
  }

  /*
   * Tolerate clients hanging up without a TLS "shutdown".  Appropriate in all
   * application protocols which perform their own message "framing", and
   * don't rely on TLS to defend against "truncation" attacks.
   */
  opts = SSL_OP_IGNORE_UNEXPECTED_EOF;

  /*
   * Block potential CPU-exhaustion attacks by clients that request frequent
   * renegotiation.  This is of course only effective if there are existing
   * limits on initial full TLS handshake or connection rates.
   */
  opts |= SSL_OP_NO_RENEGOTIATION;

  /*
   * Most servers elect to use their own cipher preference rather than that of
   * the client.
   */
  opts |= SSL_OP_CIPHER_SERVER_PREFERENCE;

  SSL_CTX_set_options(ctx, opts);

  if (SSL_CTX_use_certificate_file(ctx, get_cert_file(), SSL_FILETYPE_PEM) <=
      0) {
    SSL_CTX_free(ctx);
    assert(SSL_CTX_use_certificate_file(ctx, get_cert_file(),
                                        SSL_FILETYPE_PEM) > 0);
  }

  if (SSL_CTX_use_PrivateKey_file(ctx, get_key_file(), SSL_FILETYPE_PEM) <= 0) {
    SSL_CTX_free(ctx);
    assert(SSL_CTX_use_PrivateKey_file(ctx, get_key_file(), SSL_FILETYPE_PEM) >
           0);
  }

  SSL_CTX_set_session_id_context(ctx, (void *)cache_id, sizeof(cache_id));
  SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
  SSL_CTX_sess_set_cache_size(ctx, get_session_max_size());
  SSL_CTX_set_timeout(ctx, 1800);
  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

  ssl_array = malloc(get_poll_array_max_size() * sizeof(SSL *));
  if (ssl_array == NULL) {
    SSL_CTX_free(ctx);
    assert(ssl_array != NULL);
  }

  bio_array = malloc(get_poll_array_max_size() * sizeof(BIO *));
  if (bio_array == NULL) {
    free(ssl_array);
    SSL_CTX_free(ctx);
    assert(bio_array != NULL);
  }

  for (int i = 0; i < get_poll_array_max_size(); i++) {
    bio_array[i] = BIO_new(BIO_s_socket());
    if (bio_array[i] == NULL) {
      for (int j = 0; j < i; j++) {
        BIO_free(bio_array[j]);
      }
      free(bio_array);
      free(ssl_array);
      SSL_CTX_free(ctx);
      assert(bio_array[i] != NULL);
    }
  }

  for (int i = 0; i < get_poll_array_max_size(); i++) {
    ssl_array[i] = SSL_new(ctx);
    if (ssl_array[i] == NULL) {
      for (int j = 0; j < i; j++) {
        SSL_free(ssl_array[j]);
      }
      for (int j = 0; j < get_poll_array_max_size(); j++) {
        BIO_free(bio_array[j]);
      }
      free(bio_array);
      free(ssl_array);
      SSL_CTX_free(ctx);
      assert(ssl_array[i] != NULL);
    }
  }
}

int handle_accept(int client_fd) {
  BIO_new_socket(client_fd, BIO_NOCLOSE);
  SSL *ssl = ssl_array[client_fd];
  BIO *bio = bio_array[client_fd];
  BIO_set_fd(bio, client_fd, BIO_NOCLOSE);
  SSL_set_bio(ssl, bio, bio);

  if (SSL_accept(ssl) <= 0) {
    log_error("SSL_accept failed");
    return -1;
  }

  return 0;
}

void destroy_ssl_listener() {
  for (int i = 0; i < get_poll_array_max_size(); i++) {
    SSL_free(ssl_array[i]);
    BIO_free(bio_array[i]);
  }
  free(ssl_array);
  free(bio_array);
  SSL_CTX_free(ctx);
}

void listen_async_ssl(int socket_fd) {
  assert(socket_fd > 0);
  init_ssl_listener();
  assert(ctx != NULL);
  assert(ssl_array != NULL);
  assert(bio_array != NULL);
  log_info("Listening on SSL port %d", get_https_port());
}
