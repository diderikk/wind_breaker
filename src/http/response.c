#include "response.h"
#include "static.h"
#include <string.h>
#include <stdio.h>

char *http_status_code_to_str(http_status_code status_code);
int http_response_to_str(http_response_t *http_response, char *response_str, size_t response_str_size);
int validate_request_headers(http_request_t *http_request);

int construct_response(http_request_t *http_request, char *response){
    int return_value;
    http_response_t http_response;

    http_response.status_code = validate_request_headers(http_request);
    strcpy(http_response.content_type, "text/html;charset=utf-8");
    strcpy(http_response.content_language, "en-US");
    if(http_response.status_code != HTTP_OK){
        str(http_response.body, "<html><body><h1>404 Not Found</h1></body></html>");
        http_response.content_length = strlen(http_response.body);
        return_value = http_response_to_str(&http_response, response, HTTP_BODY_SIZE);
        return return_value;
    }
    strcpy(http_response.body, "<html><body><h1>Hello, World!</h1></body></html>"); 
    http_response.content_length = strlen(http_response.body);

    return_value = http_response_to_str(&http_response, response, HTTP_BODY_SIZE);

    return return_value;
}

int validate_request_headers(http_request_t *http_request){
    if(http_request->method != HTTP_GET){
        return HTTP_METHOD_NOT_ALLOWED;
    }

    
    return HTTP_OK;
}

int http_response_to_str(http_response_t *http_response, char *response_str, size_t response_str_size){
    char *status_code_str = http_status_code_to_str(http_response->status_code);
    int offset = 0;
    
    offset += snprintf(response_str + offset, response_str_size - offset, "HTTP/%s %d %s\r\n", HTTP_VERSION, http_response->status_code, status_code_str);

    offset += snprintf(response_str + offset, response_str_size - offset, "Content-Type: %s\r\n", http_response->content_type);

    offset += snprintf(response_str + offset, response_str_size - offset, "Content-Length: %ld\r\n", http_response->content_length);

    offset += snprintf(response_str + offset, response_str_size - offset, "\r\n");

    if(offset + http_response->content_length >= response_str_size){
        perror("Response Buffer Overflow");
        return -1;
    }

    offset += snprintf(response_str + offset, response_str_size - offset, "%s", http_response->body);


    return 0;
}

char *http_status_code_to_str(http_status_code status_code){
    switch(status_code){
        case HTTP_OK:
            return "OK";
        case HTTP_BAD_REQUEST:
            return "Bad Request";
        case HTTP_NOT_FOUND:
            return "Not Found";
        case HTTP_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
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
