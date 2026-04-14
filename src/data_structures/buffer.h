#ifndef BUFFER_H
#define BUFFER_H

#define DEFAULT_BUFFER_SIZE 1024
#define ENSURE_CAPACITY(buffer, capacity)                                      \
  ensure_capacity_(__FILE__, buffer, capacity)

typedef struct {
  unsigned long capacity;
  unsigned long count;
  char *data;
} buffer;

buffer *init_buffer(unsigned long initial_capacity);
void deinit_buffer(buffer *buffer_);

int copy_buffer(buffer *dest, buffer *src);

int ensure_capacity_(const char *callee, buffer *buffer,
                     unsigned long capacity);

#endif // BUFFER_H
