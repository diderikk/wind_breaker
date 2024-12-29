#include "http_cases.h"
#include "../http/static.h"
#include "../socket.h"
#include "../utils/assert2.h"
#include "connection_cases.h"
#include <string.h>
#include <unistd.h>

int http_request_to_string(const http_request_t *request, char *buffer);
char *http_method_to_string(http_method method);

void *start_http_get_request(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  char buffer[REQUEST_RESPONSE_MAX_SIZE];
  int socket_fd, request_length, response_length;

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

  request_length = http_request_to_string(&request, buffer);
  log_trace("Sending request: %s", buffer);
  assert(send_socket(socket_fd, buffer, request_length) > 0);

  memset(buffer, 0, REQUEST_RESPONSE_MAX_SIZE);

  response_length = recv_socket(socket_fd, buffer, REQUEST_RESPONSE_MAX_SIZE);
  log_trace("Server response: %s", buffer);

  close(socket_fd);

  assert(response_length > 0);
  assert(strstr(buffer, "HTTP/1.1 200 OK") != NULL);
  assert(strstr(buffer, "Content-Type: text/html") != NULL);
  assert(strstr(buffer, "Content-Length: 68") != NULL);
  assert(strstr(buffer, "Content-Language: en-US") != NULL);
  assert(strstr(buffer, "Content-Encoding: ") == NULL);

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
                 http_method_to_string(request->method), request->uri,
                 request->version, request->host, request->user_agent,
                 request->accept, request->accept_language,
                 request->accept_encoding,
                 request->connection == KEEP_ALIVE ? "keep-alive" : "close");
}

char *http_method_to_string(http_method method) {
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
