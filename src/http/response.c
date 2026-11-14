#define _GNU_SOURCE
#include "response.h"
#include "../utils/assert2.h"
#include "../utils/compression.h"
#include "../utils/hash.h"
#include "../utils/logger.h"
#include "../utils/static_file.h"
#include <string.h>
#include <time.h>

char *http_status_code_to_str(HTTP_STATUS_CODE status_code);
unsigned int to_string(const http_response_t *http_response, const buffer *body,
                       buffer *dest);
unsigned int set_content_length(http_response_t *http_response,
                                const unsigned int content_length);
int set_content_type(http_response_t *http_response, const uri_token_t uri);
unsigned int set_last_modified(http_response_t *http_response,
                               const uri_token_t uri);
unsigned int set_date(http_response_t *http_response);
unsigned int set_etag(http_response_t *http_response, const buffer *tmp_body);
unsigned int set_location(http_response_t *http_response, const char *host,
                          const uri_token_t uri);
unsigned int set_compression(http_response_t *http_response,
                             const char *accept_encoding, const buffer *body,
                             buffer *tmp_buffer);
void handle_if_none_match(http_response_t *http_response,
                          const char *if_none_match, buffer *tmp_buffer);

unsigned int construct_response(http_response_t *http_response,
                                const uri_token_t uri,
                                const char *accept_encoding,
                                const char *if_none_match, buffer *body,
                                buffer *tmp_buffer) {
  assert(body != NULL);

  unsigned int return_value;

  // Set content language
  strcpy(http_response->content_language, "en-US");
  // Set body
  assert(copy_buffer(tmp_buffer, body) >= 0);
  // Set content length
  set_content_length(http_response, body->count);
  // Set content type
  set_content_type(http_response, uri);
  // Set last modified
  set_last_modified(http_response, uri);
  // Set date
  set_date(http_response);
  // Set etag
  set_etag(http_response, tmp_buffer);
  // Handle if-none-match
  handle_if_none_match(http_response, if_none_match, tmp_buffer);

  if (http_response->status_code != HTTP_NOT_MODIFIED) {
    // Set compression
    unsigned int compressed_size =
        set_compression(http_response, accept_encoding, body, tmp_buffer);
    // Update content length
    set_content_length(http_response, compressed_size);
  }

  return_value = to_string(http_response, tmp_buffer, body);
  if (return_value >= 0)
    body->count = return_value;

  return return_value;
}

unsigned int construct_upgrade_to_https_response(const uri_token_t uri,
                                                 const char *host,
                                                 buffer *buffer) {
  unsigned int return_value;
  http_response_t http_response;

  // Set status code
  http_response.status_code = HTTP_MOVED_PERMANENTLY;
  // Set content language
  strcpy(http_response.content_language, "en-US");
  // Set date
  set_date(&http_response);
  // Set location
  set_location(&http_response, host, uri);
  // Set content length
  set_content_length(&http_response, 0);

  return_value = to_string(&http_response, NULL, buffer);
  buffer->count = return_value;

  return return_value;
}

int set_content_type(http_response_t *http_response, const uri_token_t uri) {
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

unsigned int set_location(http_response_t *http_response, const char *host,
                          const uri_token_t uri) {
  if (http_response->status_code != HTTP_MOVED_PERMANENTLY) {
    strcpy(http_response->location, "");
    return -1;
  } else {
    snprintf(http_response->location, HTTP_HEADER_SIZE, "https://%s%s", host,
             uri[0]); // TODO: Use env variable and join uri
  }

  return 0;
}

unsigned int set_last_modified(http_response_t *http_response,
                               const uri_token_t uri) {
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

unsigned int set_date(http_response_t *http_response) {
  time_t current_time = time(NULL);
  struct tm *time_info = gmtime(&current_time);
  return strftime(http_response->date, HTTP_HEADER_SMALL_SIZE,
                  "%a, %d %b %Y %H:%M:%S GMT", time_info);
}

unsigned int set_etag(http_response_t *http_response, const buffer *tmp_body) {
  if (http_response->status_code != HTTP_OK ||
      http_response->status_code == HTTP_NOT_MODIFIED) {
    strcpy(http_response->etag, "");
    return 0;
  } else {
    return sha256_hash_hex(tmp_body->data, tmp_body->count,
                           http_response->etag);
  }
}

void handle_if_none_match(http_response_t *http_response,
                          const char *if_none_match, buffer *tmp_buffer) {
  if (strlen(if_none_match) == 0) {
    return;
  }

  if (strcmp(if_none_match, http_response->etag) == 0) {
    http_response->status_code = HTTP_NOT_MODIFIED;
    http_response->content_length = 0;
    strcpy(http_response->content_type, "");
    strcpy(http_response->content_language, "");
    strcpy(http_response->content_encoding, "");
    memset(tmp_buffer->data, 0, tmp_buffer->capacity);
    tmp_buffer->count = 0;
  }
}

unsigned int set_compression(http_response_t *http_response,
                             const char *accept_encoding, const buffer *body,
                             buffer *tmp_buffer) {
  unsigned int return_value = http_response->content_length;
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
    return_value = compress_gzip(body->data, body->count, tmp_buffer->data);
  } else if (header_count == 2) {
    strcpy(http_response->content_encoding, "deflate");
    return_value = compress_deflate(body->data, body->count, tmp_buffer->data);
  }

  // If compression fails, return the uncompressed data.
  if (return_value < 0) {
    strcpy(http_response->content_encoding, "");
    return_value = http_response->content_length;
  }
  tmp_buffer->count = return_value;

  return return_value;
}

unsigned int set_content_length(http_response_t *http_response,
                                unsigned int content_length) {
  http_response->content_length = content_length;
  return content_length;
}

unsigned int to_string(const http_response_t *http_response, const buffer *body,
                       buffer *dest) {
  char *status_code_str = http_status_code_to_str(http_response->status_code);
  unsigned int offset = 0;

  assert(ENSURE_CAPACITY(dest, 128) > 0);

  offset += snprintf(dest->data + offset, dest->capacity - offset,
                     "HTTP/%s %d %s\r\n", HTTP_VERSION,
                     http_response->status_code, status_code_str);

  if (strlen(http_response->content_type) > 0) {
    assert(ENSURE_CAPACITY(dest, offset + 348) > 0);
    offset += snprintf(dest->data + offset, dest->capacity - offset,
                       "Content-Type: %s\r\n", http_response->content_type);
  }

  if (http_response->content_length >= 0) {
    assert(ENSURE_CAPACITY(dest, offset + 128) > 0);
    offset +=
        snprintf(dest->data + offset, dest->capacity - offset,
                 "Content-Length: %ld\r\n", http_response->content_length);
  }

  if (strlen(http_response->content_language) > 0) {
    assert(ENSURE_CAPACITY(dest, offset + 256) > 0);
    offset +=
        snprintf(dest->data + offset, dest->capacity - offset,
                 "Content-Language: %s\r\n", http_response->content_language);
  }

  if (strlen(http_response->date) > 0) {
    assert(ENSURE_CAPACITY(dest, offset + 256) > 0);
    offset += snprintf(dest->data + offset, dest->capacity - offset,
                       "Date: %s\r\n", http_response->date);
  }

  if (strlen(http_response->etag) > 0) {
    assert(ENSURE_CAPACITY(dest, offset + 128) > 0);
    offset += snprintf(dest->data + offset, dest->capacity - offset,
                       "ETag: %s\r\n", http_response->etag);
  }

  if (strlen(http_response->location) > 0) {
    assert(ENSURE_CAPACITY(dest, offset + 348) > 0);
    offset += snprintf(dest->data + offset, dest->capacity - offset,
                       "Location: %s\r\n", http_response->location);
  }

  if (strlen(http_response->last_modified) > 0) {
    assert(ENSURE_CAPACITY(dest, offset + 256) > 0);
    offset += snprintf(dest->data + offset, dest->capacity - offset,
                       "Last-Modified: %s\r\n", http_response->last_modified);
  }

  if (strlen(http_response->content_encoding) > 0) {
    assert(ENSURE_CAPACITY(dest, offset + 128) > 0);
    offset +=
        snprintf(dest->data + offset, dest->capacity - offset,
                 "Content-Encoding: %s\r\n", http_response->content_encoding);
  }

  assert(ENSURE_CAPACITY(dest, offset + http_response->content_length + 2) > 0);

  offset += snprintf(dest->data + offset, dest->capacity - offset, "\r\n");

  if (http_response->content_length > 0) {
    memcpy(dest->data + offset, body->data, http_response->content_length);
    offset += http_response->content_length;
  }

  return offset;
}

char *http_status_code_to_str(HTTP_STATUS_CODE status_code) {
  switch (status_code) {
  case HTTP_OK:
    return "OK";
  case HTTP_MOVED_PERMANENTLY:
    return "Moved Permanently";
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
