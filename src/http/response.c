#define _GNU_SOURCE
#include "response.h"
#include "../utils/logger.h"
#include "../utils/assert2.h"
#include "static.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *http_status_code_to_str(http_status_code status_code);
size_t to_string(http_response_t *http_response, char *response_str,
                 size_t response_str_size);
int validate_request_headers(http_request_t *http_request);
char *uri_to_file_name(char *uri);
size_t set_body(http_response_t *http_response, http_request_t *http_request,
                char *tmp_buffer);
size_t set_content_length(http_response_t *http_response,
                          size_t content_length);
int set_content_type(http_response_t *http_response,
                     http_request_t *http_request);
size_t set_compression(http_response_t *http_response,
                       http_request_t *http_request, char *tmp_buffer);
void log_response(http_response_t *http_response, char *tmp_body);

size_t construct_response(http_request_t *http_request, char *response, char* tmp_body_buffer) {
  assert(tmp_body_buffer != NULL);
  assert(http_request != NULL);

  size_t return_value;
  http_response_t http_response;

  // Set status code
  http_response.status_code = validate_request_headers(http_request);
  // Set content language
  strcpy(http_response.content_language, "en-US");

  // Set body
  size_t content_size = set_body(&http_response, http_request, tmp_body_buffer);
  // Set content length
  set_content_length(&http_response, content_size);
  // Set content type
  set_content_type(&http_response, http_request);

  // Set compression
  size_t compressed_size =
      set_compression(&http_response, http_request, tmp_body_buffer);
  // Set content length
  set_content_length(&http_response, compressed_size);

  return_value = to_string(&http_response, response, HTTP_BODY_SIZE);

  log_response(&http_response, tmp_body_buffer);

  return return_value;
}

size_t set_body(http_response_t *http_response, http_request_t *http_request,
                char *tmp_buffer) {
  int return_value = 0;
  if (http_response->status_code != HTTP_OK) {
    char *status_code_str = http_status_code_to_str(http_response->status_code);
    snprintf(tmp_buffer, HTTP_BODY_SIZE,
             "<html><body><h1>%d %s</h1></body></html>",
             http_response->status_code, status_code_str);

    return_value = strlen(tmp_buffer);
  } else {
    char *file_name = uri_to_file_name(http_request->uri);
    return_value = read_static_file(file_name, tmp_buffer, HTTP_BODY_SIZE);
  }
  return return_value;
}

int set_content_type(http_response_t *http_response,
                     http_request_t *http_request) {
  if (http_response->status_code != HTTP_OK) {
    strcpy(http_response->content_type, "text/html;charset=utf-8");
  } else {
    char *file_name = uri_to_file_name(http_request->uri);

    if (strcasestr(file_name, ".html") != NULL) {
      snprintf(http_response->content_type, HTTP_HEADER_SIZE,
               "text/html;charset=utf-8");
    } else if (strcasestr(file_name, ".png") != NULL) {
      snprintf(http_response->content_type, HTTP_HEADER_SIZE, "image/png");
    } else if (strcasestr(file_name, ".css") != NULL) {
      snprintf(http_response->content_type, HTTP_HEADER_SIZE,
               "text/css;charset=utf-8");
    } else if (strcasestr(file_name, ".js") != NULL) {
      snprintf(http_response->content_type, HTTP_HEADER_SIZE,
               "application/javascript;charset=utf-8");
    } else {
      snprintf(http_response->content_type, HTTP_HEADER_SIZE,
               "text/plain;charset=utf-8");
    }
  }
  return 0;
}

size_t set_compression(http_response_t *http_response,
                       http_request_t *http_request, char *tmp_buffer) {
  size_t return_value = http_response->content_length;
  int header_count = 0;
  // Validate header
  if (strcasestr(http_request->accept_encoding, "gzip") != NULL) {
    header_count += 1;
  }
  if (strcasestr(http_request->accept_encoding, "deflate") != NULL) {
    header_count += 2;
  }

  if (header_count == 1 || header_count == 3) {
    strcpy(http_response->content_encoding, "gzip");
    return_value = compress_gzip(tmp_buffer, http_response->content_length,
                                 http_response->body);
  } else if (header_count == 2) {
    strcpy(http_response->content_encoding, "deflate");
    return_value = compress_deflate(tmp_buffer, http_response->content_length,
                                    http_response->body);
  } else {
    memcpy(http_response->body, tmp_buffer, http_response->content_length);
  }

  // If compression fails, return the uncompressed data.
  if (return_value < 0) {
    memcpy(http_response->body, tmp_buffer, http_response->content_length);
    return_value = http_response->content_length;
  }

  return return_value;
}

size_t set_content_length(http_response_t *http_response,
                          size_t content_length) {
  http_response->content_length = content_length;
  return content_length;
}

int validate_request_headers(http_request_t *http_request) {
  if (http_request->method != HTTP_GET) {
    return HTTP_METHOD_NOT_ALLOWED;
  }

  // TODO: Validate Accept encoding header and version data.
  // Accept encoding can also fallback to plain text if not supported.

  // Validate Accept-Language header.
  if (strcasestr(http_request->accept_language, "en-US") == NULL)
    return HTTP_NOT_ACCEPTABLE;

  // Validate URI to file path.
  char *file_name = uri_to_file_name(http_request->uri);
  if (strcmp(http_request->uri, "/") == 0) {
    if (file_name == NULL || !find_static_file(file_name))
      return HTTP_NOT_FOUND;

  } else if (strcmp(http_request->uri, "/favicon") == 0) {
    if (file_name == NULL || !find_static_file(file_name))
      return HTTP_NOT_FOUND;

  } else {
    return HTTP_NOT_FOUND;
  }

  // Validate Accept header based on file type.
  if (strcasestr(file_name, ".html") != NULL) {
    if (strcasestr(http_request->accept, "text/html") == NULL &&
        strcasestr(http_request->accept, "text/*") == NULL &&
        strcasestr(http_request->accept, "*/*") == NULL) {
      return HTTP_NOT_ACCEPTABLE;
    }
  } else if (strcasestr(file_name, ".png") != NULL) {
    if (strcasestr(http_request->accept, "*/*") == NULL &&
        strcasestr(http_request->accept, "image/*") == NULL &&
        strcasestr(http_request->accept, "image/png") == NULL)
      return HTTP_NOT_ACCEPTABLE;
  } else if (strcasestr(file_name, ".css") != NULL) {
    if (strcasestr(http_request->accept, "text/css") == NULL &&
        strcasestr(http_request->accept, "text/*") == NULL &&
        strcasestr(http_request->accept, "*/*") == NULL)
      return HTTP_NOT_ACCEPTABLE;
  } else if (strcasestr(file_name, ".js") != NULL) {
    if (strcasestr(http_request->accept, "application/javascript") == NULL &&
        strcasestr(http_request->accept, "application/*") == NULL &&
        strcasestr(http_request->accept, "*/*") == NULL)
      return HTTP_NOT_ACCEPTABLE;
  } else {
    return HTTP_NOT_ACCEPTABLE;
  }

  return HTTP_OK;
}

size_t to_string(http_response_t *http_response, char *response_str,
                 size_t response_str_size) {
  char *status_code_str = http_status_code_to_str(http_response->status_code);
  size_t offset = 0;

  offset += snprintf(response_str + offset, response_str_size - offset,
                     "HTTP/%s %d %s\r\n", HTTP_VERSION,
                     http_response->status_code, status_code_str);

  offset += snprintf(response_str + offset, response_str_size - offset,
                     "Content-Type: %s\r\n", http_response->content_type);

  offset += snprintf(response_str + offset, response_str_size - offset,
                     "Content-Length: %ld\r\n", http_response->content_length);

  offset +=
      snprintf(response_str + offset, response_str_size - offset,
               "Content-Language: %s\r\n", http_response->content_language);

  if (strlen(http_response->content_encoding) > 0) {
    offset +=
        snprintf(response_str + offset, response_str_size - offset,
                 "Content-Encoding: %s\r\n", http_response->content_encoding);
  }

  offset += snprintf(response_str + offset, response_str_size - offset, "\r\n");

  if (offset + http_response->content_length >= response_str_size) {
    log_error("Response Buffer Overflow");
    return -1;
  }

  memcpy(response_str + offset, http_response->body,
         http_response->content_length);
  offset += http_response->content_length;

  return offset;
}

void log_response(http_response_t *http_response, char *tmp_body) {
  log_info("Responding:\n Status Code: %d\n Content Type: %s\n "
           "Content Length: %ld\n Content Language: %s\n "
           "Content Encoding: %s\n Body: %s",
           http_response->status_code, http_response->content_type,
           http_response->content_length, http_response->content_language,
           http_response->content_encoding, tmp_body);
}

char *http_status_code_to_str(http_status_code status_code) {
  switch (status_code) {
  case HTTP_OK:
    return "OK";
  case HTTP_BAD_REQUEST:
    return "Bad Request";
  case HTTP_NOT_FOUND:
    return "Not Found";
  case HTTP_METHOD_NOT_ALLOWED:
    return "Method Not Allowed";
  case HTTP_NOT_ACCEPTABLE:
    return "Not Acceptable";
  case HTTP_INTERNAL_SERVER_ERROR:
    return "Internal Server Error";
  case HTTP_NOT_IMPLEMENTED:
    return "Not Implemented";
  case HTTP_SERVICE_UNAVAILABLE:
    return "Service Unavailable";
  default:
    return "Unknown";
  }
}

char *uri_to_file_name(char *uri) {
  if (strcmp(uri, "/") == 0) {
    return "index.html";
  } else if (strcmp(uri, "/favicon") == 0) {
    return "favicon.png";
  } else {
    return NULL;
  }
}
