#include "ssl.h"
#include "../properties.h"
#include "assert2.h"

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

  if (SSL_CTX_use_certificate_chain_file(ctx, get_cert_file()) <= 0) {
    SSL_CTX_free(ctx);
    assert(SSL_CTX_use_certificate_chain_file(ctx, get_cert_file()) > 0);
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

void destroy_ssl_ctx() {
  assert(ctx != NULL);
  SSL_CTX_free(ctx);
  ctx = NULL;
}
