#include "ssl_array.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"

static SSL **ssl_array = NULL;
static BIO **bio_array = NULL;
static int max_size = 0;
static int count = 0;

void init_ssl_array(SSL_CTX *ctx, int _max_size) {
  max_size = _max_size;

  ssl_array = malloc(max_size * sizeof(SSL *));
  if (ssl_array == NULL) {
    SSL_CTX_free(ctx);
    assert(ssl_array != NULL);
  }

  bio_array = malloc(max_size * sizeof(BIO *));
  if (bio_array == NULL) {
    free(ssl_array);
    SSL_CTX_free(ctx);
    assert(bio_array != NULL);
  }

  for (int i = 0; i < max_size; i++) {
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

  for (int i = 0; i < max_size; i++) {
    ssl_array[i] = SSL_new(ctx);
    if (ssl_array[i] == NULL) {
      for (int j = 0; j < i; j++) {
        SSL_free(ssl_array[j]);
      }
      for (int j = 0; j < max_size; j++) {
        BIO_free(bio_array[j]);
      }
      free(bio_array);
      free(ssl_array);
      SSL_CTX_free(ctx);
      assert(ssl_array[i] != NULL);
    }
  }
}

SSL *get_ssl_by_index(int index) {
  assert(ssl_array != NULL);
  assert(index > 0);
  assert(index < max_size);
  assert(index < count);

  return ssl_array[index];
}

BIO *get_bio_by_index(int index) {
  assert(bio_array != NULL);
  assert(index > 0);
  assert(index < max_size);
  assert(index < count);

  return bio_array[index];
}

void add_ssl_by_index(int index) {
  assert(ssl_array != NULL);
  assert(index > 0);
  assert(index < max_size);
  assert(index < count);

  log_trace("Adding SSL connection %d", index);

  if (count < max_size)
    count++;

  assert(count <= max_size);
}

void remove_ssl_by_index(int index) {
  assert(ssl_array != NULL);
  assert(index > 0);
  assert(index < max_size);
  assert(index < count);

  log_trace("Removing SSL connection %d", index);

  SSL *ssl = ssl_array[index];
  BIO *bio = bio_array[index];
  for (int i = index; i < max_size - 1; i++) {
    ssl_array[i] = ssl_array[i + 1];
    bio_array[i] = bio_array[i + 1];
  }
  ssl_array[max_size - 1] = ssl;
  bio_array[max_size - 1] = bio;

  count--;
  assert(count >= 0);
  assert(ssl_array[max_size - 1] != NULL);
  assert(bio_array[max_size - 1] != NULL);
}

void destroy_ssl_array() {
  assert(ssl_array != NULL);
  assert(bio_array != NULL);
  assert(max_size > 0);
  for (int i = 0; i < max_size; i++) {
    SSL_free(ssl_array[i]);
    ssl_array[i] = NULL;
    BIO_free(bio_array[i]);
    bio_array[i] = NULL;
  }
  free(ssl_array);
  ssl_array = NULL;
  free(bio_array);
  bio_array = NULL;
}
