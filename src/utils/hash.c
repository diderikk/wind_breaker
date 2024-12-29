#include "hash.h"
#include "assert2.h"
#include "logger.h"
#include <openssl/evp.h>

int sha256_hash_hex(const char *input, const long input_length,
                    char outputBuffer[SHA256_DIGEST_LENGTH * 2 + 1]) {
  EVP_MD_CTX *mdctx;
  unsigned char digest[SHA256_DIGEST_LENGTH];
  unsigned int digest_length;

  assert((mdctx = EVP_MD_CTX_new()) != NULL);
  assert(EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) == 1);
  assert(EVP_DigestUpdate(mdctx, input, input_length) == 1);
  assert(EVP_DigestFinal_ex(mdctx, digest, &digest_length) == 1);

  EVP_MD_CTX_free(mdctx);

  for (int i = 0; i < digest_length; i++) {
    sprintf(outputBuffer + (i * 2), "%02x", digest[i]);
  }
  outputBuffer[SHA256_DIGEST_LENGTH * 2] = '\0';

  log_trace("Hashed %ld bytes a SHA256 digest: %s", input_length, outputBuffer);

  return SHA256_DIGEST_LENGTH * 2 + 1;
}
