#ifndef HTTP_CASES_H
#define HTTP_CASES_H

void *start_http_get_request(void *arg);
void *start_http_get_request_gzip(void *arg);
void *start_http_get_request_deflate(void *arg);
void *start_http_get_request_not_found(void *arg);

#endif // HTTP_CASES_H
