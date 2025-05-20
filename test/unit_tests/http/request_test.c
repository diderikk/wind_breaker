#include "../../../src/utils/assert2.h"
#include "../../../src/http/request.h"
#include <string.h>

static int request_test_count = 0;
static int request_start_case(void *(*func)(void *), const char *name);

// Example GET requests as C strings
static const char *request1 = "GET / HTTP/1.1\r\n"
                       "Host: localhost\r\n"
                       "User-Agent: test-agent\r\n"
                       "Accept: text/html\r\n"
                       "Accept-Language: en-US\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n";

static const char *request2 = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.82 Safari/537.36\r\n"
                       "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
                       "Accept-Language: en-US,en;q=0.5\r\n"
                       "Accept-Encoding: gzip, deflate, br\r\n"
                       "Connection: keep-alive\r\n"
                       "Upgrade-Insecure-Requests: 1\r\n"
                       "\r\n";

static const char *request3 = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "User-Agent: test-agent\r\n"
                       "Accept: */*\r\n"
                       "Accept-Language: en-US\r\n"
                       "Accept-Encoding: gzip, deflate\r\n"
                       "Connection: keep-alive\r\n"
                       "If-None-Match: \"etag-value\"\r\n"
                       "\r\n";

static const char *request4 = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "User-Agent: test-agent\r\n"
                       "Accept: application/json\r\n"
                       "Accept-Language: en-US\r\n"
                       "Accept-Encoding: gzip, deflate\r\n"
                       "Connection: keep-alive\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n";

static const char *request5 = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "\r\n";

static const char *request6 = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "User-Agent: test-agent\r\n"
                       "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
                       "Accept-Language: en-US,en;q=0.9\r\n"
                       "Accept-Encoding: gzip, deflate, sdch\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n";

static const char *request7 = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "User-Agent: test-agent\r\n"
                       "Accept: text/html\r\n"
                       "Accept-Language: en-US\r\n"
                       "Accept-Encoding: gzip, deflate\r\n"
                       "Connection: close\r\n"
                       "\r\n";

static const char *request8 = "GET /abc/def/ghe HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "User-Agent: test-agent\r\n"
                       "Accept: text/html\r\n"
                       "Accept-Language: en-US\r\n"
                       "Accept-Encoding: gzip, deflate\r\n"
                       "Connection: close\r\n"
                       "\r\n";

// Test function for basic GET request
void* parse_http_request_basic_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request1);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "localhost") == 0);
    assert(strcmp(http_request.user_agent, "test-agent") == 0);
    assert(strcmp(http_request.accept, "text/html") == 0);
    assert(strcmp(http_request.accept_language, "en-US") == 0);
    assert(http_request.connection == KEEP_ALIVE);

    return NULL;
}

// Test function for GET request with additional headers
void* parse_http_request_with_additional_headers_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request2);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "example.com") == 0);
    assert(strcmp(http_request.user_agent, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.82 Safari/537.36") == 0);
    assert(strcmp(http_request.accept, "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8") == 0);
    assert(strcmp(http_request.accept_language, "en-US,en;q=0.5") == 0);
    assert(strcmp(http_request.accept_encoding, "gzip, deflate, br") == 0);
    assert(http_request.connection == KEEP_ALIVE);

    return NULL;
}

// Test function for GET request with If-None-Match header
void* parse_http_request_if_none_match_header_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request3);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "example.com") == 0);
    assert(strcmp(http_request.user_agent, "test-agent") == 0);
    assert(strcmp(http_request.accept, "*/*") == 0);
    assert(strcmp(http_request.accept_language, "en-US") == 0);
    assert(strcmp(http_request.accept_encoding, "gzip, deflate") == 0);
    assert(http_request.connection == KEEP_ALIVE);
    assert(strcmp(http_request.if_none_match, "\"etag-value\"") == 0);

    return NULL;
}

// Test function for GET request with Content-Type and Content-Length headers
void* parse_http_request_content_type_and_length_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request4);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "example.com") == 0);
    assert(strcmp(http_request.user_agent, "test-agent") == 0);
    assert(strcmp(http_request.accept, "application/json") == 0);
    assert(strcmp(http_request.accept_language, "en-US") == 0);
    assert(strcmp(http_request.accept_encoding, "gzip, deflate") == 0);
    assert(http_request.connection == KEEP_ALIVE);
    assert(strcmp(http_request.content_type, "application/json") == 0);
    assert(http_request.content_length == 0);

    return NULL;
}

// Test function for minimal GET request
void* parse_http_request_minimal_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request5);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "example.com") == 0);

    return NULL;
}

// Test function for GET request with complex Accept header
void* parse_http_request_with_complex_accept_header_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request6);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "example.com") == 0);
    assert(strcmp(http_request.user_agent, "test-agent") == 0);
    assert(strcmp(http_request.accept, "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8") == 0);
    assert(strcmp(http_request.accept_language, "en-US,en;q=0.9") == 0);
    assert(strcmp(http_request.accept_encoding, "gzip, deflate, sdch") == 0);
    assert(http_request.connection == KEEP_ALIVE);

    return NULL;
}

// Test function for GET request with Connection close
void* parse_http_request_with_connection_close_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request7);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "example.com") == 0);
    assert(strcmp(http_request.user_agent, "test-agent") == 0);
    assert(strcmp(http_request.accept, "text/html") == 0);
    assert(strcmp(http_request.accept_language, "en-US") == 0);
    assert(strcmp(http_request.accept_encoding, "gzip, deflate") == 0);
    assert(http_request.connection == CLOSE);

    return NULL;
}

void* parse_http_request_with_multiple_path_parameters_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request_t));
    char buffer[REQUEST_RESPONSE_MAX_SIZE];
    strcpy(buffer, request8);

    int result = parse_http_request(&http_request, buffer);

    assert(result == 0);
    assert(http_request.method == HTTP_GET);
    assert(strcmp(http_request.uri[0], "abc") == 0);
    assert(strcmp(http_request.uri[1], "def") == 0);
    assert(strcmp(http_request.uri[2], "ghe") == 0);
    assert(strcmp(http_request.version, "HTTP/1.1") == 0);
    assert(strcmp(http_request.host, "example.com") == 0);
    assert(strcmp(http_request.user_agent, "test-agent") == 0);
    assert(strcmp(http_request.accept, "text/html") == 0);
    assert(strcmp(http_request.accept_language, "en-US") == 0);
    assert(strcmp(http_request.accept_encoding, "gzip, deflate") == 0);
    assert(http_request.connection == CLOSE);

    return NULL;
}

void* validate_request_headers_valid_get_request_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request));

    // Populate the http_request structure with valid values
    http_request.method = HTTP_GET;
    strcpy(http_request.uri[0], "");
    strcpy(http_request.host, "localhost");
    strcpy(http_request.user_agent, "test-agent");
    strcpy(http_request.accept, "text/html");
    strcpy(http_request.accept_language, "en-US");
    strcpy(http_request.accept_encoding, "gzip, deflate");
    http_request.connection = KEEP_ALIVE;

    int result = validate_request_headers(&http_request);
    assert(result == HTTP_OK);

    return NULL;
}

void* validate_request_headers_method_not_allowed_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request));

    // Populate the http_request structure with an unsupported method
    http_request.method = HTTP_POST;
    strcpy(http_request.uri[0], "");
    strcpy(http_request.host, "localhost");
    strcpy(http_request.user_agent, "test-agent");
    strcpy(http_request.accept, "text/html");
    strcpy(http_request.accept_language, "en-US");
    strcpy(http_request.accept_encoding, "gzip, deflate");
    http_request.connection = KEEP_ALIVE;

    int result = validate_request_headers(&http_request);
    assert(result == HTTP_METHOD_NOT_ALLOWED);

    return NULL;
}

void* validate_request_headers_language_not_supported_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request));

    // Populate the http_request structure with an unsupported language
    http_request.method = HTTP_GET;
    strcpy(http_request.uri[0], "");
    strcpy(http_request.host, "localhost");
    strcpy(http_request.user_agent, "test-agent");
    strcpy(http_request.accept, "text/html");
    strcpy(http_request.accept_language, "fr-FR");
    strcpy(http_request.accept_encoding, "gzip, deflate");
    http_request.connection = KEEP_ALIVE;

    int result = validate_request_headers(&http_request);
    assert(result == HTTP_NOT_ACCEPTABLE);

    return NULL;
}

void* validate_request_headers_encoding_not_supported_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request));

    // Populate the http_request structure with an unsupported encoding
    http_request.method = HTTP_GET;
    strcpy(http_request.uri[0], "");
    strcpy(http_request.host, "localhost");
    strcpy(http_request.user_agent, "test-agent");
    strcpy(http_request.accept, "text/html");
    strcpy(http_request.accept_language, "en-US");
    strcpy(http_request.accept_encoding, "br");
    http_request.connection = KEEP_ALIVE;

    int result = validate_request_headers(&http_request);
    assert(result == HTTP_NOT_ACCEPTABLE);

    return NULL;
}

void* validate_request_headers_file_not_found_test() {
    http_request_t http_request;
    memset(&http_request, 0, sizeof(http_request));

    // Populate the http_request structure with a non-existent URI
    http_request.method = HTTP_GET;
    strcpy(http_request.uri[0], "nonexistent");
    strcpy(http_request.host, "localhost");
    strcpy(http_request.user_agent, "test-agent");
    strcpy(http_request.accept, "text/html");
    strcpy(http_request.accept_language, "en-US");
    strcpy(http_request.accept_encoding, "gzip, deflate");
    http_request.connection = KEEP_ALIVE;

    int result = validate_request_headers(&http_request);
    assert(result == HTTP_NOT_FOUND);

    return NULL;
}

void * uri_test() {
  uri_token_t uri1 = {0};
  uri_token_t uri2 = {0};
  strcpy(uri1[0], "");
  strcpy(uri2[0], "favicon");


  assert(strcmp(uri_to_file_name(uri1), "index.html") == 0);
  assert(strcmp(uri_to_file_name(uri2), "favicon.png") == 0);

  return NULL;
}

int request_test() {
  request_start_case(uri_test, "uri_test");
  request_start_case(parse_http_request_basic_test, "parse_http_request_basic_test");
  request_start_case(parse_http_request_with_additional_headers_test, "parse_http_request_with_additional_headers_test");
  request_start_case(parse_http_request_if_none_match_header_test, "parse_http_request_if_none_match_header_test");
  request_start_case(parse_http_request_content_type_and_length_test, "parse_http_request_content_type_and_length_test");
  request_start_case(parse_http_request_minimal_test, "parse_http_request_minimal_test");
  request_start_case(parse_http_request_with_complex_accept_header_test, "parse_http_request_with_complex_accept_header_test");
  request_start_case(parse_http_request_with_connection_close_test, "parse_http_request_with_connection_close_test");
  request_start_case(validate_request_headers_valid_get_request_test, "validate_request_headers_valid_get_request_test");
  request_start_case(validate_request_headers_method_not_allowed_test, "validate_request_headers_method_not_allowed_test");
  request_start_case(validate_request_headers_language_not_supported_test, "validate_request_headers_language_not_supported_test");
  request_start_case(validate_request_headers_encoding_not_supported_test, "validate_request_headers_encoding_not_supported_test");
  request_start_case(validate_request_headers_file_not_found_test, "validate_request_headers_file_not_found_test");

  return request_test_count;
}

static int request_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", request_test_count++, name);

  func(NULL);

  return 0;
}
