#include "../../../src/http/response.h"
#include "../../../src/static.h"
#include "../../../src/utils/assert2.h"
#include "../../../src/utils/hash.h"
#include "../../../src/utils/static_file.h"
#include <string.h>

// Large enough for the compressed output, which set_compression writes into
// the tmp buffer without growing it.
#define TEST_BUFFER_SIZE (64 * 1024)

// Buffers are not guaranteed to be NUL-terminated after a response is built
// (the body is memcpy'd last), so search with an explicit length.
static int contains(const buffer *haystack, const char *needle) {
  return memmem(haystack->data, haystack->count, needle, strlen(needle)) !=
         NULL;
}

static buffer *make_body(const char *text) {
  buffer *body = init_buffer(TEST_BUFFER_SIZE);
  assert(body != NULL);
  body->count = strlen(text);
  memcpy(body->data, text, body->count);
  return body;
}

static int response_test_count = 0;
static int response_start_case(void *(*func)(void *), const char *name);

void *construct_response_ok_test() {
  buffer *body = make_body(
      "<html><body><h1>Hello, World from Static folder!</h1></body></html>");
  buffer *tmp = init_buffer(TEST_BUFFER_SIZE);
  uri_token_t uri = {0};

  http_response_t *http_response = calloc(1, sizeof(http_response_t));
  http_response->status_code = HTTP_OK;

  size_t response_size =
      construct_response(http_response, uri, "", "", body, tmp);

  assert(response_size > 0);
  assert(response_size == body->count);
  assert(contains(body, "HTTP/1.1 200 OK"));
  assert(contains(body, "Content-Type: text/html;charset=utf-8"));
  assert(!contains(body, "Content-Encoding: gzip"));
  assert(contains(body, "Content-Language: en-US"));
  assert(contains(body, "Content-Length: "));
  assert(contains(body, "Date: "));
  assert(contains(body, "Last-Modified: "));
  assert(contains(body, "ETag: "));
  assert(contains(body, "<body>"));

  free(http_response);
  deinit_buffer(body);
  deinit_buffer(tmp);
  return NULL;
}

void *construct_response_ok_with_compression_test() {
  buffer *body = make_body(
      "<html><body><h1>Hello, World from Static folder!</h1></body></html>");
  buffer *tmp = init_buffer(TEST_BUFFER_SIZE);
  uri_token_t uri = {0};

  http_response_t *http_response = calloc(1, sizeof(http_response_t));
  http_response->status_code = HTTP_OK;

  size_t response_size =
      construct_response(http_response, uri, "gzip", "", body, tmp);

  assert(response_size > 0);
  assert(contains(body, "HTTP/1.1 200 OK"));
  assert(contains(body, "Content-Type: text/html;charset=utf-8"));
  assert(contains(body, "Content-Encoding: gzip"));
  assert(contains(body, "Content-Language: en-US"));
  assert(contains(body, "Content-Length: "));
  assert(contains(body, "Date: "));
  assert(contains(body, "Last-Modified: "));
  assert(contains(body, "ETag: "));
  assert(!contains(body, "<body>"));

  free(http_response);
  deinit_buffer(body);
  deinit_buffer(tmp);
  return NULL;
}

void *construct_response_not_found_test() {
  buffer *body = make_body("<html><body><h1>404</h1></body></html>");
  buffer *tmp = init_buffer(TEST_BUFFER_SIZE);
  uri_token_t uri = {0};
  strcpy(uri[0], "nonexistent");

  http_response_t *http_response = calloc(1, sizeof(http_response_t));
  http_response->status_code = HTTP_NOT_FOUND;

  size_t response_size =
      construct_response(http_response, uri, "", "", body, tmp);

  assert(response_size > 0);
  assert(contains(body, "HTTP/1.1 404 Not Found"));
  assert(contains(body, "Content-Type: text/html;charset=utf-8"));

  free(http_response);
  deinit_buffer(body);
  deinit_buffer(tmp);
  return NULL;
}

void *construct_response_bad_request_test() {
  buffer *body = make_body("<html><body><h1>400</h1></body></html>");
  buffer *tmp = init_buffer(TEST_BUFFER_SIZE);
  uri_token_t uri = {0};

  http_response_t *http_response = calloc(1, sizeof(http_response_t));
  http_response->status_code = HTTP_BAD_REQUEST;

  size_t response_size =
      construct_response(http_response, uri, "", "", body, tmp);

  assert(response_size > 0);
  assert(contains(body, "HTTP/1.1 400 Bad Request"));
  assert(contains(body, "Content-Type: text/html;charset=utf-8"));

  free(http_response);
  deinit_buffer(body);
  deinit_buffer(tmp);
  return NULL;
}

void *construct_response_not_modified_test() {
  buffer *body = init_buffer(TEST_BUFFER_SIZE);
  buffer *tmp = init_buffer(TEST_BUFFER_SIZE);
  char etag[SHA256_DIGEST_LENGTH * 2 + 1] = {0};
  uri_token_t uri = {0};

  assert(read_static_file("index.html", body) > 0);
  sha256_hash_hex(body->data, body->count, etag);

  http_response_t *http_response = calloc(1, sizeof(http_response_t));
  http_response->status_code = HTTP_OK;

  size_t response_size =
      construct_response(http_response, uri, "gzip", etag, body, tmp);

  assert(response_size > 0);
  assert(contains(body, "HTTP/1.1 304 Not Modified"));
  assert(contains(body, "Date: "));
  assert(contains(body, "Content-Length: 0"));
  assert(contains(body, "Last-Modified: "));
  assert(contains(body, "ETag: "));
  assert(contains(body, etag));

  assert(!contains(body, "Content-Type: "));
  assert(!contains(body, "Content-Encoding: "));
  assert(!contains(body, "<body>"));

  free(http_response);
  deinit_buffer(body);
  deinit_buffer(tmp);
  return NULL;
}

int response_test() {
  response_start_case(construct_response_ok_test, "construct_response_ok_test");
  response_start_case(construct_response_ok_with_compression_test,
                      "construct_response_ok_with_compression_test");
  response_start_case(construct_response_not_found_test,
                      "construct_response_not_found_test");
  response_start_case(construct_response_bad_request_test,
                      "construct_response_bad_request_test");
  response_start_case(construct_response_not_modified_test,
                      "construct_response_not_modified_test");

  return response_test_count;
}

static int response_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", response_test_count++, name);

  func(NULL);

  return 0;
}
