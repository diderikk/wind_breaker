#ifndef HANDLER_HELPERS_H
#define HANDLER_HELPERS_H

#include "../static.h"
#include "../utils/assert2.h"
#include "../utils/bit.h"
#include <stddef.h> // Ensure NULL is defined

static inline unsigned int seek_buffer_offset(const char *buffer) {
  assert(buffer != NULL);
  unsigned int offset = 0;
  while (!(buffer[offset] == 0 && buffer[offset + 1] == 0 &&
           buffer[offset + 2] == 0 && buffer[offset + 3] == 0)) {
    offset += 4 + combine_chars(buffer[offset], buffer[offset + 1],
                                buffer[offset + 2], buffer[offset + 3]);
  }

  assert(offset >= 0);
  return offset;
}

static inline unsigned int append_response_length(unsigned int offset,
                                                  char *buffer,
                                                  unsigned int response_size) {
  assert(buffer != NULL);
  assert(response_size > 0);
  assert(offset >= 0);

  split_int(response_size, buffer, offset);

  assert(!(buffer[offset] == 0 && buffer[offset + 1] == 0 &&
           buffer[offset + 2] == 0 && buffer[offset + 3] == 0));
  return offset + 4;
}

static inline void buffer_move_to_front(unsigned int offset, char *buffer) {
  assert(buffer != NULL);
  assert(offset > 4);

  unsigned int start_offset = offset;
  while (!(buffer[offset] == 0 && buffer[offset + 1] == 0 &&
           buffer[offset + 2] == 0 && buffer[offset + 3] == 0)) {
    offset += 4 + combine_chars(buffer[offset], buffer[offset + 1],
                                buffer[offset + 2], buffer[offset + 3]);
  }
  if (start_offset == offset) {
    memset(buffer, 0, REQUEST_RESPONSE_MAX_SIZE);
  } else {
    memmove(buffer, buffer + start_offset, offset - start_offset);
    memset(buffer + offset - start_offset, 0,
           REQUEST_RESPONSE_MAX_SIZE - (offset - start_offset));
  }
}

#endif // HANDLER_HELPERS_H
