#define _GNU_SOURCE
#include "request.h"
#include "../utils/logger.h"
#include "../utils/regex2.h"
#include "../utils/static_file.h"
#include <stdio.h>
#include <string.h>

int parse_control_data(http_request_t *http_request, char *char_data);
int parse_header_field(http_request_t *http_request, char *char_data);
char *uri_to_file_name(char *uri);
// int parse_trailer_fields(http_request_t* http_request, char * char_data);
// int extract_body(http_request_t* http_request, char * char_data, long
// content_size);
http_method method_str_to_enum(char *raw_method);

int parse_request(http_request_t *http_request, char *raw_request) {
  log_trace("Raw Request:\n %s", raw_request);
  int return_value;
  return_value = parse_http_request(http_request, raw_request);

  if (return_value != 0)
    return return_value;

  return 0;
}
int parse_http_request(http_request_t *http_request, char *raw_request) {
  int return_value;
  char *line = strtok(raw_request, "\r\n");
  return_value = parse_control_data(http_request, line);

  if (return_value != 0)
    return return_value;

  log_info("extracted control data => method: %d, uri: %s, version: %s",
           http_request->method, http_request->uri, http_request->version);

  while ((line = strtok(NULL, "\r\n")) != NULL) {
    return_value = parse_header_field(http_request, line);
    if (return_value != 0)
      return return_value;
  }

  log_info("extracted header fields => host: %s, user_agent: %s, accept: %s, "
           "accept_language: %s, accept_encoding: %s, connection: %d",
           http_request->host, http_request->user_agent, http_request->accept,
           http_request->accept_language, http_request->accept_encoding,
           http_request->connection);

  return 0;
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

char *uri_to_file_name(char *uri) {
  if (strcmp(uri, "/") == 0) {
    return "index.html";
  } else if (strcmp(uri, "/favicon") == 0) {
    return "favicon.png";
  } else {
    return NULL;
  }
}

int parse_control_data(http_request_t *http_request, char *raw_control_data) {
  http_method method;
  const char *pattern = "^([A-Z]{2,12}) ([^ ]+) (HTTP/[0-9.]{3})$";
  regmatch_t matches[4]; // Method, URI, Version

  int matches_count = match_regex(pattern, raw_control_data, 4, matches, 0);

  if (matches_count != 4)
    return matches_count;

  if (matches[2].rm_eo - matches[2].rm_so >= HTTP_URI_SIZE) {
    log_error("URI Too Big");
    return -1;
  }

  if (matches[3].rm_eo - matches[3].rm_so >= HTTP_VERSION_SIZE) {
    log_error("Version Too Big");
    return -1;
  }

  char method_str[HTTP_METHOD_SIZE];
  strncpy(method_str, raw_control_data + matches[1].rm_so,
          matches[1].rm_eo - matches[1].rm_so);
  method_str[matches[1].rm_eo - matches[1].rm_so] = '\0';
  http_request->method = method_str_to_enum(method_str);
  method = method_str_to_enum(raw_control_data + matches[1].rm_so);
  // HTTP Method
  http_request->method = method;

  // HTTP URI
  strncpy(http_request->uri, raw_control_data + matches[2].rm_so,
          matches[2].rm_eo - matches[2].rm_so);
  http_request->uri[matches[2].rm_eo - matches[2].rm_so] = '\0';

  // HTTP Version
  strncpy(http_request->version, raw_control_data + matches[3].rm_so,
          matches[3].rm_eo - matches[3].rm_so);
  http_request->uri[matches[3].rm_eo - matches[3].rm_so] = '\0';

  return 0;
}

// Extracts the header field from the raw data and stores it in the http_request
// Only one header field is extracted at a time.
int parse_header_field(http_request_t *http_request, char *raw_header_field) {
  const char *pattern = "^([A-Za-z0-9-]+):\\s(.*)$";
  regmatch_t matches[3]; // Header Name, Header Value
  char header_name[100];

  int matches_count = match_regex(pattern, raw_header_field, 3, matches, 0);

  if (matches_count != 3)
    return matches_count;

  if (matches[1].rm_eo - matches[1].rm_so >= 100) {
    log_error("HTTP Header name Too Big");
    return -1;
  }

  if (matches[2].rm_eo - matches[2].rm_so >= HTTP_HEADER_SIZE) {
    log_error("Header value Too Big");
    return -1;
  }

  strncpy(header_name, raw_header_field + matches[1].rm_so,
          matches[1].rm_eo - matches[1].rm_so);
  header_name[matches[1].rm_eo - matches[1].rm_so] = '\0';

  if (strcmp(header_name, "Host") == 0) {
    strncpy(http_request->host, raw_header_field + matches[2].rm_so,
            matches[2].rm_eo - matches[2].rm_so);
    http_request->host[matches[2].rm_eo - matches[2].rm_so] = '\0';
  } else if (strcmp(header_name, "User-Agent") == 0) {
    strncpy(http_request->user_agent, raw_header_field + matches[2].rm_so,
            matches[2].rm_eo - matches[2].rm_so);
    http_request->user_agent[matches[2].rm_eo - matches[2].rm_so] = '\0';
  } else if (strcmp(header_name, "Accept") == 0) {
    strncpy(http_request->accept, raw_header_field + matches[2].rm_so,
            matches[2].rm_eo - matches[2].rm_so);
    http_request->accept[matches[2].rm_eo - matches[2].rm_so] = '\0';
  } else if (strcmp(header_name, "Accept-Language") == 0) {
    strncpy(http_request->accept_language, raw_header_field + matches[2].rm_so,
            matches[2].rm_eo - matches[2].rm_so);
    http_request->accept_language[matches[2].rm_eo - matches[2].rm_so] = '\0';
  } else if (strcmp(header_name, "Accept-Encoding") == 0) {
    strncpy(http_request->accept_encoding, raw_header_field + matches[2].rm_so,
            matches[2].rm_eo - matches[2].rm_so);
    http_request->accept_encoding[matches[2].rm_eo - matches[2].rm_so] = '\0';
  } else if (strcmp(header_name, "Connection") == 0) {
    if (strncmp(raw_header_field + matches[2].rm_so, "keep-alive", 10) == 0)
      http_request->connection = KEEP_ALIVE;
    else if (strncmp(raw_header_field + matches[2].rm_so, "close", 5) == 0)
      http_request->connection = CLOSE;
  }

  return 0;
}

http_method method_str_to_enum(char *raw_method) {
  if (strncmp(raw_method, "POST", 4) == 0)
    return HTTP_POST;
  else if (strncmp(raw_method, "GET", 3) == 0)
    return HTTP_GET;
  else if (strncmp(raw_method, "PUT", 3) == 0)
    return HTTP_PUT;
  else if (strncmp(raw_method, "DELETE", 6) == 0)
    return HTTP_DELETE;
  else if (strncmp(raw_method, "HEAD", 4) == 0)
    return HTTP_HEAD;
  else if (strncmp(raw_method, "OPTIONS", 7) == 0)
    return HTTP_OPTIONS;
  else if (strncmp(raw_method, "PATCH", 5) == 0)
    return HTTP_PATCH;
  else if (strncmp(raw_method, "CONNECT", 7) == 0)
    return HTTP_CONNECT;
  else if (strncmp(raw_method, "TRACE", 5) == 0)
    return HTTP_TRACE;

  return HTTP_BAD_METHOD;
}
