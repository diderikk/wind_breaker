#include "listener_ssl.h"
#include "data_structures/session.h"
#include "data_structures/worker_queue.h"
#include "listener.h"
#include "properties.h"
#include "socket.h"
#include "static.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include <openssl/ssl.h>

// https://github.com/openssl/openssl/blob/master/demos/guide/tls-server-block.c

static SSL_CTX *ctx = NULL;
static const unsigned char cache_id[] = "wind_breaker_server";

SSL_CTX *init_ssl_ctx() {
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

  return ctx;
}

int static inline handle_ssl_except_error(SSL *ssl, int ret) {
  int err = SSL_get_error(ssl, ret);
  // According to the SSL_accept, non-blocking socket must be handled
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
    if (err == SSL_ERROR_WANT_READ)
      log_debug("SSL_ERROR_WANT_READ");
    else
      log_debug("SSL_ERROR_WANT_WRITE");
    return 0;
  } else {
    log_error("SSL_accept failed");
    return -1;
  }
}

int handle_accept(SSL *ssl, BIO *bio) {

  if ((SSL_in_init(ssl) && !SSL_in_before(ssl)) || SSL_is_server(ssl)) {
    log_trace("Shutting down ongoing SSL connection %d", index - 1);

    int ret = SSL_shutdown(ssl);
    if (ret == 0) {
      // Shutdown is not yet complete, call SSL_shutdown() again
      ret = SSL_shutdown(ssl);
    }

    if (ret != 1) {
      log_warn("SSL_shutdown failed");
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
  }

  SSL_set_bio(ssl, bio, bio);

  // TODO: Handle trying to access port using http should return with 301,
  // location to with https prefix
  int ret = SSL_accept(ssl);
  if (ret <= 0 || ret == 2) {
    return handle_ssl_except_error(ssl, ret);
  }

  return 0;
}

void handle_close(int fd) {
  struct session_full_return session = get_session_sync(fd);
  int ret;
  assert(session.ssl != NULL);
  if (SSL_in_init(session.ssl) || SSL_is_server(session.ssl)) {
    log_trace("Shutting down ongoing SSL connection %d", index - 1);

    ret = SSL_shutdown(session.ssl);
    if (ret == 0) {
      // Shutdown is not yet complete, call SSL_shutdown() again
      ret = SSL_shutdown(session.ssl);
    }

    if (ret != 1) {
      log_error("SSL_shutdown failed");
    }
  }

  // Does not handle SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
  // Forcing a reset of the SSL object. File descriptor is closed by _listen.
  // poll_array implementation will try to find a fd that is not POLLIN or
  // POLLOUT Reset the SSL and BIO objects for reuse
  ret = SSL_clear(session.ssl);
  if (ret != 1) {
    log_error("SSL_clear failed");
  }
}

int handle_ssl_request_async(int fd, char *buffer) {
  struct session_full_return session = get_session_sync(fd);
  assert(session.session != NULL);
  assert(session.bio != NULL);
  assert(session.ssl != NULL);
  if (!SSL_is_init_finished(session.ssl)) {
    int ret = SSL_accept(session.ssl);
    if (ret <= 0 || ret == 2) {
      return handle_ssl_except_error(session.ssl, ret);
    } else {
      log_trace("SSL_accept success");
      return 0;
    }
  }

  int recv_return = recv_bio(session.bio, buffer, REQUEST_RESPONSE_MAX_SIZE);
  if (recv_return > 0) {
    queue_push(fd, buffer, recv_return);
    return 0;
  } else if (recv_return == -1 && BIO_should_retry(session.bio) == 1) {
    return 0;
  } else {
    // Got error or connection closed by client
    if (recv_return == 0) {
      // Connection closed
      log_trace("Socket %d hung up", fd);
    }
    return -1;
  }
}

void destroy_ssl_ctx() {
  assert(ctx != NULL);
  SSL_CTX_free(ctx);
  ctx = NULL;
}

void *listen_async_ssl(int *listener_fd) {
  assert(*listener_fd > 0);
  assert(ctx != NULL);
  _listen(*listener_fd, handle_accept, handle_close, handle_ssl_request_async);
  return NULL;
}
