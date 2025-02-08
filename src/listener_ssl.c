#include "data_structures/ssl_array.h"
#include "listener.h"
#include "properties.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

// https://github.com/openssl/openssl/blob/master/demos/guide/tls-server-block.c

static SSL_CTX *ctx = NULL;
static const unsigned char cache_id[] = "wind_breaker_server";

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

  init_ssl_array(ctx, get_poll_array_max_size() - 1);
}

int handle_accept(int client_fd, int index) {
  SSL *ssl = get_ssl_by_index(index);
  BIO *bio = get_bio_by_index(index);

  if (SSL_in_init(ssl)) {
    log_trace("Shutting down ongoing SSL connection %d", index - 1);

    int ret = SSL_shutdown(ssl);
    if (ret == 0) {
      // Shutdown is not yet complete, call SSL_shutdown() again
      ret = SSL_shutdown(ssl);
    }

    if (ret != 1) {
      log_error("SSL_shutdown failed");
      return -1; // TODO: Maybe remove?
    }

    // Does not handle SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
    // Forcing a reset of the SSL object. File descriptor is closed by _listen.
    // poll_array implementation will try to find a fd that is not POLLIN or
    // POLLOUT Reset the SSL and BIO objects for reuse
    ret = SSL_clear(ssl);
    if (ret != 1) {
      log_error("SSL_clear failed");
      return -1;
    }
    ret = BIO_reset(bio);
    if (ret != 1) {
      log_error("BIO_reset failed");
      return -1;
    }
  }

  BIO_set_fd(bio, client_fd, BIO_NOCLOSE);
  SSL_set_bio(ssl, bio, bio);
  add_ssl_by_index(index);

  int ret = SSL_accept(ssl);
  if (ret <= 0 || ret == 2) {
    log_error("SSL_accept failed");
    if (ret <= 0)
      return -1;
  }

  return 0;
}

void handle_close(int index) {
  SSL *ssl = get_ssl_by_index(index);
  BIO *bio = get_bio_by_index(index);

  if (SSL_in_init(ssl)) {
    log_trace("Shutting down ongoing SSL connection %d", index - 1);

    int ret = SSL_shutdown(ssl);
    if (ret == 0) {
      // Shutdown is not yet complete, call SSL_shutdown() again
      ret = SSL_shutdown(ssl);
    }

    if (ret != 1) {
      log_error("SSL_shutdown failed");
    }

    // Does not handle SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
    // Forcing a reset of the SSL object. File descriptor is closed by _listen.
    // poll_array implementation will try to find a fd that is not POLLIN or
    // POLLOUT Reset the SSL and BIO objects for reuse
    ret = SSL_clear(ssl);
    if (ret != 1) {
      log_error("SSL_clear failed");
    }
    ret = BIO_reset(bio);
    if (ret != 1) {
      log_error("BIO_reset failed");
    }
  }
  remove_ssl_by_index(index);
}

int handle_ssl_request_async(int index, char *buffer) { return 0; }

void destroy_ssl_listener() {
  assert(ctx != NULL);
  destroy_ssl_array();
  SSL_CTX_free(ctx);
  ctx = NULL;
}

void listen_async_ssl(int socket_fd) {
  assert(socket_fd > 0);
  init_ssl_listener();
  assert(ctx != NULL);
}
