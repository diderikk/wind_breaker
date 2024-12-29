#ifndef HASH_H
#define HASH_H

#include <openssl/sha.h>

int sha256_hash_hex(const char *input, const long input_length,
                    char outputBuffer[SHA256_DIGEST_LENGTH * 2 + 1]);

#endif // HASH_H
