#include "../../../src/utils/assert2.h"
#include "../../../src/utils/logger.h"
#include "../../../src/utils/hash.h"
#include "../../../src/utils/static_file.h"
#include "../../../src/http/response.h"


static int response_test_count = 0;
static int response_start_case(void *(*func)(void *), const char *name);

void* construct_response_ok_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    char tmp_body_buffer[HTTP_BODY_SIZE] = {0};

    size_t response_size = construct_response(HTTP_OK, "/", "", "", response, tmp_body_buffer);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(response, "Content-Type: text/html;charset=utf-8") != NULL);
    assert(strstr(response, "Content-Encoding: gzip") == NULL);
    assert(strstr(response, "Content-Language: en-US") != NULL);
    assert(strstr(response, "Content-Length: ") != NULL);
    assert(strstr(response, "Date: ") != NULL);
    assert(strstr(response, "Last-Modified: ") != NULL);
    assert(strstr(response, "ETag: ") != NULL);
    assert(strstr(response, "<body>") != NULL);

    return NULL;
}

void* construct_response_ok_with_compression_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    char tmp_body_buffer[HTTP_BODY_SIZE] = {0};

    size_t response_size = construct_response(HTTP_OK, "/", "gzip", "", response, tmp_body_buffer);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(response, "Content-Type: text/html;charset=utf-8") != NULL);
    assert(strstr(response, "Content-Encoding: gzip") != NULL);
    assert(strstr(response, "Content-Language: en-US") != NULL);
    assert(strstr(response, "Content-Length: ") != NULL);
    assert(strstr(response, "Date: ") != NULL);
    assert(strstr(response, "Last-Modified: ") != NULL);
    assert(strstr(response, "ETag: ") != NULL);
    assert(strstr(response, "<body>") == NULL);

    return NULL;
}

void* construct_response_not_found_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    char tmp_body_buffer[HTTP_BODY_SIZE] = {0};

    size_t response_size = construct_response(HTTP_NOT_FOUND, "/nonexistent", "", "", response, tmp_body_buffer);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 404 Not Found") != NULL);
    assert(strstr(response, "Content-Type: text/html;charset=utf-8") != NULL);

    return NULL;
}

void* construct_response_bad_request_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    char tmp_body_buffer[HTTP_BODY_SIZE] = {0};

    size_t response_size = construct_response(HTTP_BAD_REQUEST, "/", "", "", response, tmp_body_buffer);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
    assert(strstr(response, "Content-Type: text/html;charset=utf-8") != NULL);

    return NULL;
}

void* construct_response_not_modified_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    char tmp_body_buffer[HTTP_BODY_SIZE] = {0};
    char etag[SHA256_DIGEST_LENGTH * 2 + 1] = {0};

    size_t buff_size = read_static_file("index.html", tmp_body_buffer, HTTP_BODY_SIZE);
    sha256_hash_hex(tmp_body_buffer, buff_size, etag); 

    memset(tmp_body_buffer, 0, HTTP_BODY_SIZE);

    size_t response_size = construct_response(HTTP_OK, "/", "gzip", etag, response, tmp_body_buffer);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 304 Not Modified") != NULL);
    assert(strstr(response, "Date: ") != NULL);
    assert(strstr(response, "Last-Modified: ") != NULL);
    assert(strstr(response, "ETag: ") != NULL);
    assert(strstr(response, etag) != NULL);

    assert(strstr(response, "Content-Type: ") == NULL);
    assert(strstr(response, "Content-Encoding: ") == NULL);
    assert(strstr(response, "Content-Length: ") == NULL);
    assert(strstr(response, "<body>") == NULL);

    return NULL;
}

int response_test() {
  response_start_case(construct_response_ok_test, "construct_response_ok_test");
  response_start_case(construct_response_ok_with_compression_test, "construct_response_ok_with_compression_test");
  response_start_case(construct_response_not_found_test, "construct_response_not_found_test");
  response_start_case(construct_response_bad_request_test, "construct_response_bad_request_test");
  response_start_case(construct_response_not_modified_test, "construct_response_not_modified_test");

  return response_test_count;
}

static int response_start_case(void *(*func)(void *), const char *name) {
  log_trace("Starting test %d, named: %s", response_test_count++, name);

  func(NULL);

  return 0;
}
