#include "request.h"
#include <stdio.h>
#include <string.h>

int match_regex(const char *pattern, char *text, int match_count,
                regmatch_t *matches, int flags);
int parse_control_data(http_request_t *http_request, char *char_data);
int parse_header_fields(http_request_t *http_request, char *char_data);
// int parse_trailer_fields(http_request_t* http_request, char * char_data);
// int extract_body(http_request_t* http_request, char * char_data, long
// content_size);
http_method method_str_to_enum(char *raw_method);


int handle_request(http_request_t *http_request, char *raw_request) {
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

  printf("extracted control data => method: %d, uri: %s, version: %s\n",
         http_request->method, http_request->uri, http_request->version);

  while ((line = strtok(NULL, "\r\n")) != NULL) {
    return_value = parse_header_fields(http_request, line);
    if (return_value != 0)
      return return_value;
  }

  printf("extracted header fields => host: %s, user_agent: %s, accept: %s, "
         "accept_language: %s, accept_encoding: %s, connection: %d\n",
         http_request->host, http_request->user_agent, http_request->accept,
         http_request->accept_language, http_request->accept_encoding,
         http_request->connection);

  return 0;
}

int parse_control_data(http_request_t *http_request, char *raw_control_data) {
  http_method method;
  const char *pattern = "^([A-Z]{2,12}) ([^ ]+) (HTTP/[0-9.]{3})$";
  regmatch_t matches[4]; // Method, URI, Version

  int matches_count = match_regex(pattern, raw_control_data, 4, matches, 0);

  if (matches_count != 4)
    return matches_count;

  if (matches[2].rm_eo - matches[2].rm_so >= HTTP_URI_SIZE) {
    perror("URI Too Big");
    return -1;
  }

  if (matches[3].rm_eo - matches[3].rm_so >= HTTP_VERSION_SIZE) {
    perror("Version Too Big");
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

int parse_header_fields(http_request_t *http_request, char *raw_header_field) {
  const char *pattern = "^([A-Za-z0-9-]+):\\s(.*)$";
  regmatch_t matches[3]; // Header Name, Header Value
  char header_name[100];

  int matches_count = match_regex(pattern, raw_header_field, 3, matches, 0);

  if (matches_count != 3)
    return matches_count;

  if (matches[1].rm_eo - matches[1].rm_so >= 100) {
    perror("HTTP Header name Too Big");
    return -1;
  }

  if (matches[2].rm_eo - matches[2].rm_so >= HTTP_HEADER_SIZE) {
    perror("Header value Too Big");
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

int match_regex(const char *pattern, char *text, int match_count,
                regmatch_t *matches, int flags) {
  regex_t regex;
  int ret = regcomp(&regex, pattern, REG_EXTENDED);
  if (ret) {
    fprintf(stderr, "Could not compile regex\n");
    return -1;
  }

  // Execute the regular expression
  ret = regexec(&regex, text, match_count, matches, flags);
  if (!ret) {
    // printf("Match found:\n");

    // Print the entire match
    //        for (int i = 0; i < match_count; i++) {
    //            if (matches[i].rm_so != -1) {
    //                printf("Match %d: %.*s\n", i, matches[i].rm_eo -
    //                matches[i].rm_so, text + matches[i].rm_so);
    //            }
    //        }
  } else if (ret == REG_NOMATCH) {
    printf("No match\n");
    return -1;
  } else {
    char errbuf[100];
    regerror(ret, &regex, errbuf, sizeof(errbuf));
    fprintf(stderr, "Regex match failed: %s\n", errbuf);
    return -1;
  }

  // Free the compiled regular expression
  regfree(&regex);

  return match_count;
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
