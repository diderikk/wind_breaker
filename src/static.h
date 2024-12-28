#ifndef STATIC_H
#define STATIC_H

#include "http/static.h"

typedef struct {
  int fd;
  char data[HTTP_BODY_SIZE];
} request_data;

#endif // STATIC_H
