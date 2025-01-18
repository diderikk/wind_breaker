#define _GNU_SOURCE
#include "response.h"
#include "../utils/assert2.h"
#include "../utils/hash.h"
#include "../utils/logger.h"
#include "request.h"
#include "static.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

char *http_status_code_to_str(http_status_code status_code);
size_t to_string(const http_response_t *http_response, char *response_str);
size_t set_body(http_response_t *http_response, const char *uri,
                char *tmp_buffer);
size_t set_content_length(http_response_t *http_response,
                          const size_t content_length);
int set_content_type(http_response_t *http_response, const char *uri);
size_t set_last_modified(http_response_t *http_response, const char *uri);
size_t set_date(http_response_t *http_response);
size_t set_etag(http_response_t *http_response, const char *tmp_body,
                const size_t tmp_body_size);
size_t set_compression(http_response_t *http_response,
                       const char *accept_encoding, char *tmp_buffer);
void handle_if_none_match(http_response_t *http_response,
                          const char *if_none_match);

size_t construct_response(int response_code, const char *uri,
                          const char *accept_encoding,
                          const char *if_none_match, char *response,
                          char *tmp_body_buffer) {
  assert(tmp_body_buffer != NULL);

  size_t return_value;
  http_response_t http_response;

  // Set status code
  http_response.status_code = response_code;
  // Set content language
  strcpy(http_response.content_language, "en-US");

  // Set body
  size_t content_size = set_body(&http_response, uri, tmp_body_buffer);
  // Set content length
  set_content_length(&http_response, content_size);
  // Set content type
  set_content_type(&http_response, uri);
  // Set last modified
  set_last_modified(&http_response, uri);
  // Set date
  set_date(&http_response);
  // Set etag
  set_etag(&http_response, tmp_body_buffer, content_size);
  // Handle if-none-match
  handle_if_none_match(&http_response, if_none_match);

  if (http_response.status_code != HTTP_NOT_MODIFIED) {
    // Set compression
    size_t compressed_size =
        set_compression(&http_response, accept_encoding, tmp_body_buffer);
    // Update content length
    set_content_length(&http_response, compressed_size);
  }

  return_value = to_string(&http_response, response);

  log_info("Responding:\n%s", response);

  return return_value;
}

size_t set_body(http_response_t *http_response, const char *uri,
                char *tmp_buffer) {
  int return_value = 0;
  if (http_response->status_code != HTTP_OK) {
    const char *status_code_str =
        http_status_code_to_str(http_response->status_code);
    snprintf(tmp_buffer, HTTP_BODY_SIZE,
             "<html><body><h1>%d %s</h1></body></html>",
             http_response->status_code, status_code_str);

    return_value = strlen(tmp_buffer);
  } else {
    const char *file_name = uri_to_file_name(uri);
    return_value = read_static_file(file_name, tmp_buffer, HTTP_BODY_SIZE);
  }
  return return_value;
}

int set_content_type(http_response_t *http_response, const char *uri) {
  if (http_response->status_code != HTTP_OK) {
    strcpy(http_response->content_type, "text/html;charset=utf-8");
  } else {
    const char *file_name = uri_to_file_name(uri);

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

size_t set_last_modified(http_response_t *http_response, const char *uri) {
  if (http_response->status_code != HTTP_OK) {
    strcpy(http_response->last_modified, "");
    return 0;
  } else {
    char *file_name = uri_to_file_name(uri);
    time_t last_modified_gm_time = get_last_modified(file_name);

    struct tm *time_info = gmtime(&last_modified_gm_time);
    return strftime(http_response->last_modified, HTTP_HEADER_SMALL_SIZE,
                    "%a, %d %b %Y %H:%M:%S GMT", time_info);
  }
}

size_t set_date(http_response_t *http_response) {
  time_t current_time = time(NULL);
  struct tm *time_info = gmtime(&current_time);
  return strftime(http_response->date, HTTP_HEADER_SMALL_SIZE,
                  "%a, %d %b %Y %H:%M:%S GMT", time_info);
}

size_t set_etag(http_response_t *http_response, const char *tmp_body,
                size_t tmp_body_size) {
  if (http_response->status_code != HTTP_OK ||
      http_response->status_code == HTTP_NOT_MODIFIED) {
    strcpy(http_response->etag, "");
    return 0;
  } else {
    return sha256_hash_hex(tmp_body, tmp_body_size, http_response->etag);
  }
}

void handle_if_none_match(http_response_t *http_response,
                          const char *if_none_match) {
  if (strlen(if_none_match) == 0) {
    return;
  }

  if (strcmp(if_none_match, http_response->etag) == 0) {
    http_response->status_code = HTTP_NOT_MODIFIED;
    http_response->content_length = 0;
    strcpy(http_response->content_type, "");
    strcpy(http_response->content_language, "");
    strcpy(http_response->content_encoding, "");
    strcpy(http_response->body, "");
  }
}

size_t set_compression(http_response_t *http_response,
                       const char *accept_encoding, char *tmp_buffer) {
  size_t return_value = http_response->content_length;
  int header_count = 0;
  // Validate header
  if (strcasestr(accept_encoding, "gzip") != NULL) {
    header_count += 1;
  }
  if (strcasestr(accept_encoding, "deflate") != NULL) {
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
    strcpy(http_response->content_encoding, "");
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

size_t to_string(const http_response_t *http_response, char *response_str) {
  char *status_code_str = http_status_code_to_str(http_response->status_code);
  size_t offset = 0;

  offset += snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                     "HTTP/%s %d %s\r\n", HTTP_VERSION,
                     http_response->status_code, status_code_str);

  if (strlen(http_response->content_type) > 0) {
    offset +=
        snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                 "Content-Type: %s\r\n", http_response->content_type);
  }

  if (http_response->content_length > 0) {
    offset +=
        snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                 "Content-Length: %ld\r\n", http_response->content_length);
  }

  if (strlen(http_response->content_language) > 0) {
    offset +=
        snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                 "Content-Language: %s\r\n", http_response->content_language);
  }

  if (strlen(http_response->date) > 0) {
    offset +=
        snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                 "Date: %s\r\n", http_response->date);
  }

  if (strlen(http_response->etag) > 0) {
    offset +=
        snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                 "ETag: %s\r\n", http_response->etag);
  }

  if (strlen(http_response->last_modified) > 0) {
    offset +=
        snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                 "Last-Modified: %s\r\n", http_response->last_modified);
  }

  if (strlen(http_response->content_encoding) > 0) {
    offset +=
        snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                 "Content-Encoding: %s\r\n", http_response->content_encoding);
  }

  offset += snprintf(response_str + offset, REQUEST_RESPONSE_MAX_SIZE - offset,
                     "\r\n");

  if (http_response->content_length > 0) {
    if (offset + http_response->content_length >= REQUEST_RESPONSE_MAX_SIZE) {
      log_error("Response Buffer Overflow");
      return -1;
    }

    memcpy(response_str + offset, http_response->body,
           http_response->content_length);
    offset += http_response->content_length;
  }

  return offset;
}

char *http_status_code_to_str(http_status_code status_code) {
  switch (status_code) {
  case HTTP_OK:
    return "OK";
  case HTTP_NOT_MODIFIED:
    return "Not Modified";
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
