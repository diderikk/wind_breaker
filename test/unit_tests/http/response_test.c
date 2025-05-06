#include "../../../src/utils/assert2.h"
#include "../../../src/utils/hash.h"
#include "../../../src/utils/static_file.h"
#include "../../../src/http/response.h"
#include "../../../src/static.h"
#include <string.h>

static int response_test_count = 0;
static int response_start_case(void *(*func)(void *), const char *name);

void* construct_response_ok_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    unsigned int body_size = sprintf(response, "<html><body><h1>Hello, World from Static folder!</h1></body></html>");

    http_response_t *http_response = calloc(1, sizeof(http_response_t));
    http_response->status_code = HTTP_OK;

    size_t response_size = construct_response(http_response, "/", "", "", response, body_size);

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
    unsigned int body_size = sprintf(response, "<html><body><h1>Hello, World from Static folder!</h1></body></html>");

    http_response_t *http_response = calloc(1, sizeof(http_response_t));
    http_response->status_code = HTTP_OK;

    size_t response_size = construct_response(http_response, "/", "gzip", "", response, body_size);

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
    unsigned int body_size = sprintf(response, "<html><body><h1>404</h1></body></html>");

    http_response_t *http_response = calloc(1, sizeof(http_response_t));
    http_response->status_code = HTTP_NOT_FOUND;

    size_t response_size = construct_response(http_response, "/nonexistent", "", "", response, body_size);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 404 Not Found") != NULL);
    assert(strstr(response, "Content-Type: text/html;charset=utf-8") != NULL);

    return NULL;
}

void* construct_response_bad_request_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    unsigned int body_size = sprintf(response, "<html><body><h1>400</h1></body></html>");

    http_response_t *http_response = calloc(1, sizeof(http_response_t));
    http_response->status_code = HTTP_BAD_REQUEST;

    size_t response_size = construct_response(http_response, "/", "", "", response, body_size);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
    assert(strstr(response, "Content-Type: text/html;charset=utf-8") != NULL);

    return NULL;
}

void* construct_response_not_modified_test() {
    char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
    char etag[SHA256_DIGEST_LENGTH * 2 + 1] = {0};

    unsigned int buff_size = read_static_file("index.html", response, HTTP_BODY_SIZE);
    sha256_hash_hex(response, buff_size, etag); 

    /*memset(response, 0, HTTP_BODY_SIZE);*/
    //unsigned int body_size = sprintf(response, "<html><body><h1>400</h1></body></html>");

    http_response_t *http_response = calloc(1, sizeof(http_response_t));
    http_response->status_code = HTTP_OK;

    size_t response_size = construct_response(http_response, "/", "gzip", etag, response, buff_size);

    assert(response_size > 0);
    assert(strstr(response, "HTTP/1.1 304 Not Modified") != NULL);
    assert(strstr(response, "Date: ") != NULL);
    assert(strstr(response, "Content-Length: 0") != NULL);
    assert(strstr(response, "Last-Modified: ") != NULL);
    assert(strstr(response, "ETag: ") != NULL);
    assert(strstr(response, etag) != NULL);

    assert(strstr(response, "Content-Type: ") == NULL);
    assert(strstr(response, "Content-Encoding: ") == NULL);
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
  printf("Starting test %d, named: %s\n", response_test_count++, name);

  func(NULL);

  return 0;
}
