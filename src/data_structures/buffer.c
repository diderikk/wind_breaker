#include "buffer.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>

buffer *init_buffer(unsigned long initial_capacity) {
  buffer *b = malloc(sizeof(buffer));
  if (b == NULL)
    return b;

  b->capacity = initial_capacity;
  b->count = 0;
  b->data = calloc(initial_capacity, sizeof(char));
  if (b->data == NULL) {
    free(b);
    return NULL;
  }

  return b;
}

void deinit_buffer(buffer *buffer) {
  free(buffer->data);
  free(buffer);
}

void copy_buffer(buffer *dest, buffer *src) {
  int result = ENSURE_CAPACITY(dest, src->count);
  if (result < 0)
    printf("Failed to ENSURE_CAPACITY in copy_buffer");

  memcpy(dest->data, src->data, src->count);
  dest->count = src->count;
}

int ensure_capacity_(const char *callee, buffer *buffer,
                     unsigned long capacity) {
  unsigned long old_capacity = buffer->capacity;
  while (buffer->capacity < capacity) {
    buffer->capacity = (buffer->capacity < DEFAULT_BUFFER_SIZE)
                           ? DEFAULT_BUFFER_SIZE
                           : buffer->capacity * 2;
  }
  if (buffer->capacity > old_capacity) {
    buffer->data = realloc(buffer->data, sizeof(char) * buffer->capacity);
    if (buffer->data == NULL)
      return -1;
  }

  return buffer->capacity;
}
