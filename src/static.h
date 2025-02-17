#ifndef STATIC_H
#define STATIC_H

#define REQUEST_RESPONSE_MAX_SIZE 32768

typedef struct {
  int fd;
  int size;
  char data[REQUEST_RESPONSE_MAX_SIZE];
} worker_data;

#endif // STATIC_H
