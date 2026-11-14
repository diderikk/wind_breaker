#include "http_cases.h"
#include "../../src/socket.h"
#include "../../src/static.h"
#include "../../src/utils/assert2.h"
#include "../../src/utils/compression.h"
#include "connection_cases.h"
#include <string.h>
#include <unistd.h>

#define TEST_REQUEST_RESPONSE_MAX_SIZE 16 * 1024
// Safety net so a server that never answers fails the case instead of hanging
#define TEST_RECV_TIMEOUT_MS 5000

int http_request_to_string(const http_request_t *request, char *buffer);
char *http_method_to_string(HTTP_METHOD method);

void *start_http_get_request(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  char request_text[TEST_REQUEST_RESPONSE_MAX_SIZE];
  int socket_fd, request_length, response_length;
  buffer *response = init_buffer(0);
  assert(response != NULL);

  socket_fd = connect_to_server(data->ip, data->port);
  assert(socket_fd > 0);

  const http_request_t request = {
      .method = HTTP_GET,
      .uri = "/",
      .version = HTTP_VERSION,
      .host = "localhost",
      .connection = CLOSE,
      .accept = "text/html",
      .accept_language = "en-US",
      .user_agent = "Mozilla/5.0",
      .accept_encoding = "",
  };

  request_length = http_request_to_string(&request, request_text);
  assert(send_socket(socket_fd, request_text, request_length) > 0);

  response_length = recv_response(socket_fd, response, TEST_RECV_TIMEOUT_MS);

  close(socket_fd);

  assert(response_length > 0);
  assert(strstr(response->data, "HTTP/1.1 200 OK") != NULL);
  assert(strstr(response->data, "Content-Type: text/html") != NULL);
  assert(strstr(response->data, "Content-Length: ") != NULL);
  assert(strstr(response->data, "Content-Language: en-US") != NULL);
  assert(strstr(response->data, "Date: ") != NULL);
  assert(strstr(response->data, "Last-Modified: ") != NULL);
  assert(strstr(response->data, "ETag: ") != NULL);
  assert(strstr(response->data, "Content-Encoding: ") == NULL);

  deinit_buffer(response);
  return 0;
}

void *start_http_get_request_gzip(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  char request_text[TEST_REQUEST_RESPONSE_MAX_SIZE],
      decompressed[TEST_REQUEST_RESPONSE_MAX_SIZE];
  int socket_fd, request_length, response_length;
  buffer *response = init_buffer(0);
  assert(response != NULL);

  socket_fd = connect_to_server(data->ip, data->port);
  assert(socket_fd > 0);

  const http_request_t request = {
      .method = HTTP_GET,
      .uri = "/",
      .version = HTTP_VERSION,
      .host = "localhost",
      .connection = CLOSE,
      .accept = "text/html",
      .accept_language = "en-US",
      .user_agent = "Mozilla/5.0",
      .accept_encoding = "gzip",
  };

  request_length = http_request_to_string(&request, request_text);
  assert(send_socket(socket_fd, request_text, request_length) > 0);

  response_length = recv_response(socket_fd, response, TEST_RECV_TIMEOUT_MS);

  close(socket_fd);

  assert(response_length > 0);
  assert(strstr(response->data, "HTTP/1.1 200 OK") != NULL);
  assert(strstr(response->data, "Content-Type: text/html") != NULL);
  assert(strstr(response->data, "Content-Length: ") != NULL);
  assert(strstr(response->data, "Content-Language: en-US") != NULL);
  assert(strstr(response->data, "Content-Encoding: gzip") != NULL);
  assert(strstr(response->data, "Date: ") != NULL);
  assert(strstr(response->data, "Last-Modified: ") != NULL);
  assert(strstr(response->data, "ETag: ") != NULL);
  assert(strstr(response->data, "<body>") == NULL);

  const char *body = strstr(response->data, "\r\n\r\n");
  body += 4;
  unsigned int body_length = response->data + response->count - body;

  int decompressed_length = decompress_gzip(body, body_length, decompressed,
                                            TEST_REQUEST_RESPONSE_MAX_SIZE);
  assert(decompressed_length > 0);
  assert(strstr(decompressed, "<body>") != NULL);

  deinit_buffer(response);
  return 0;
}

void *start_http_get_request_deflate(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  char request_text[TEST_REQUEST_RESPONSE_MAX_SIZE],
      decompressed[TEST_REQUEST_RESPONSE_MAX_SIZE];
  int socket_fd, request_length, response_length;
  buffer *response = init_buffer(0);
  assert(response != NULL);

  socket_fd = connect_to_server(data->ip, data->port);
  assert(socket_fd > 0);

  const http_request_t request = {
      .method = HTTP_GET,
      .uri = "/",
      .version = HTTP_VERSION,
      .host = "localhost",
      .connection = CLOSE,
      .accept = "text/html",
      .accept_language = "en-US",
      .user_agent = "Mozilla/5.0",
      .accept_encoding = "deflate",
  };

  request_length = http_request_to_string(&request, request_text);
  assert(send_socket(socket_fd, request_text, request_length) > 0);

  response_length = recv_response(socket_fd, response, TEST_RECV_TIMEOUT_MS);

  close(socket_fd);

  assert(response_length > 0);
  assert(strstr(response->data, "HTTP/1.1 200 OK") != NULL);
  assert(strstr(response->data, "Content-Type: text/html") != NULL);
  assert(strstr(response->data, "Content-Length: ") != NULL);
  assert(strstr(response->data, "Content-Language: en-US") != NULL);
  assert(strstr(response->data, "Content-Encoding: deflate") != NULL);
  assert(strstr(response->data, "Date: ") != NULL);
  assert(strstr(response->data, "Last-Modified: ") != NULL);
  assert(strstr(response->data, "ETag: ") != NULL);
  assert(strstr(response->data, "<body>") == NULL);

  const char *body = strstr(response->data, "\r\n\r\n");
  body += 4;
  unsigned int body_length = response->data + response->count - body;

  int decompressed_length = decompress_deflate(body, body_length, decompressed,
                                               TEST_REQUEST_RESPONSE_MAX_SIZE);
  // printf("Decompressed: %s\n", decompressed);
  assert(decompressed_length > 0);
  assert(strstr(decompressed, "<head>") != NULL);

  deinit_buffer(response);
  return 0;
}

void *start_http_get_request_not_found(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  char request_text[TEST_REQUEST_RESPONSE_MAX_SIZE],
      decompressed[TEST_REQUEST_RESPONSE_MAX_SIZE];
  int socket_fd, request_length, response_length;
  buffer *response = init_buffer(0);
  assert(response != NULL);

  socket_fd = connect_to_server(data->ip, data->port);
  assert(socket_fd > 0);

  const http_request_t request = {
      .method = HTTP_GET,
      .uri = "/hackerman",
      .version = HTTP_VERSION,
      .host = "localhost",
      .connection = CLOSE,
      .accept = "text/html",
      .accept_language = "en-US",
      .user_agent = "Mozilla/5.0",
      .accept_encoding = "",
  };

  request_length = http_request_to_string(&request, request_text);
  assert(send_socket(socket_fd, request_text, request_length) > 0);

  response_length = recv_response(socket_fd, response, TEST_RECV_TIMEOUT_MS);

  close(socket_fd);

  assert(response_length > 0);
  assert(strstr(response->data, "HTTP/1.1 404 Not Found") != NULL);
  assert(strstr(response->data, "Content-Type: text/html") != NULL);
  assert(strstr(response->data, "Content-Length: ") != NULL);
  assert(strstr(response->data, "Content-Language: en-US") != NULL);
  assert(strstr(response->data, "Content-Encoding: ") == NULL);
  assert(strstr(response->data, "Date: ") != NULL);
  assert(strstr(response->data, "Last-Modified: ") == NULL);
  assert(strstr(response->data, "ETag: ") == NULL);
  assert(strstr(response->data, "<h1>404 Not Found</h1>") != NULL);

  deinit_buffer(response);
  return 0;
}

int http_request_to_string(const http_request_t *request, char *buffer) {
  return sprintf(buffer,
                 "%s %s HTTP/%s\r\n"
                 "Host: %s\r\n"
                 "User-Agent: %s\r\n"
                 "Accept: %s\r\n"
                 "Accept-Language: %s\r\n"
                 "Accept-Encoding: %s\r\n"
                 "Connection: %s\r\n",
                 http_method_to_string(request->method), request->uri[0],
                 request->version, request->host, request->user_agent,
                 request->accept, request->accept_language,
                 request->accept_encoding,
                 request->connection == KEEP_ALIVE ? "keep-alive" : "close");
}

char *http_method_to_string(HTTP_METHOD method) {
  switch (method) {
  case HTTP_POST:
    return "POST";
  case HTTP_GET:
    return "GET";
  case HTTP_PATCH:
    return "PATCH";
  case HTTP_PUT:
    return "PUT";
  case HTTP_HEAD:
    return "HEAD";
  case HTTP_DELETE:
    return "DELETE";
  case HTTP_CONNECT:
    return "CONNECT";
  case HTTP_OPTIONS:
    return "OPTIONS";
  case HTTP_TRACE:
    return "TRACE";
  default:
    return "BAD_METHOD";
  }
}
