#define _GNU_SOURCE
#include "response.h"
#include "../utils/compression.h"
#include "../utils/file.h"
#include "static.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *http_status_code_to_str(http_status_code status_code);
int http_response_to_str(http_response_t *http_response, char *response_str,
                         size_t response_str_size);
int validate_request_headers(http_request_t *http_request);
char *uri_to_file_name(char *uri);
void set_content_type_from_file_name(char *file_name, char *buffer,
                                     size_t buffer_size);

int construct_response(http_request_t *http_request, char *response) {
  int return_value;
  http_response_t http_response;
  char *tmp_buffer = malloc(HTTP_BODY_SIZE);
  if (!*tmp_buffer) {
    perror("Failed to allocate memory for compressed buffer");
    return -1;
  }

  // Set status code
  http_response.status_code = validate_request_headers(http_request);
  // Set content language
  strcpy(http_response.content_language, "en-US");
  if (http_response.status_code != HTTP_OK) {

    // Set content type
    strcpy(http_response.content_type, "text/html;charset=utf-8");

    char *status_code_str = http_status_code_to_str(http_response.status_code);
    snprintf(tmp_buffer, HTTP_BODY_SIZE,
             "<html><body><h1>%d %s</h1></body></html>",
             http_response.status_code, status_code_str);

    // Set content length
    http_response.content_length = strlen(http_response.body);
  } else {

    char *file_name = uri_to_file_name(http_request->uri);
    int read_size = read_static_file(file_name, tmp_buffer, HTTP_BODY_SIZE);
    // Set content type
    set_content_type_from_file_name(file_name, http_response.content_type,
                                    HTTP_HEADER_SIZE);

    // Set content length
    http_response.content_length = read_size;
  }
  int compressed_size = compress_gzip(tmp_buffer, http_response.content_length, http_response.body);
  // If compression fails, return the uncompressed data.
  // TODO: Validate header
  if(compressed_size < 0) {
    memcpy(http_response.body, tmp_buffer, http_response.content_length);
  } else {
    strcpy(http_response.content_encoding, "gzip");
    http_response.content_length = compressed_size;
  }
  return_value = http_response_to_str(&http_response, response, HTTP_BODY_SIZE);


  printf("return_value: %d\n", return_value);


  return return_value;
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

int http_response_to_str(http_response_t *http_response, char *response_str,
                         size_t response_str_size) {
  char *status_code_str = http_status_code_to_str(http_response->status_code);
  int offset = 0;

  offset += snprintf(response_str + offset, response_str_size - offset,
                     "HTTP/%s %d %s\r\n", HTTP_VERSION,
                     http_response->status_code, status_code_str);

  offset += snprintf(response_str + offset, response_str_size - offset,
                     "Content-Type: %s\r\n", http_response->content_type);

  offset += snprintf(response_str + offset, response_str_size - offset,
                     "Content-Length: %ld\r\n", http_response->content_length);

  offset += snprintf(response_str + offset, response_str_size - offset,
                     "Content-Language: %s\r\n", http_response->content_language);

  if(strlen(http_response->content_encoding) > 0){
    offset += snprintf(response_str + offset, response_str_size - offset,
                     "Content-Encoding: %s\r\n", http_response->content_encoding);
  }

  offset += snprintf(response_str + offset, response_str_size - offset, "\r\n");

  if (offset + http_response->content_length >= response_str_size) {
    perror("Response Buffer Overflow");
    return -1;
  }

  memcpy(response_str + offset, http_response->body,
         http_response->content_length);
  offset += http_response->content_length;

  return offset;
}

void set_content_type_from_file_name(char *file_name, char *buffer,
                                     size_t buffer_size) {
  if (strcasestr(file_name, ".html") != NULL) {
    snprintf(buffer, buffer_size, "text/html;charset=utf-8");
  } else if (strcasestr(file_name, ".png") != NULL) {
    snprintf(buffer, buffer_size, "image/png");
  } else if (strcasestr(file_name, ".css") != NULL) {
    snprintf(buffer, buffer_size, "text/css;charset=utf-8");
  } else if (strcasestr(file_name, ".js") != NULL) {
    snprintf(buffer, buffer_size, "application/javascript;charset=utf-8");
  } else {
    snprintf(buffer, buffer_size, "text/plain;charset=utf-8");
  }
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
