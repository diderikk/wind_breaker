#ifndef BIT_H
#define BIT_H

static inline void split_int(unsigned int integer, char *buffer,
                             unsigned int offset) {
  buffer[offset] = (integer >> 24) & 0xFF;     // Get the first byte
  buffer[offset + 1] = (integer >> 16) & 0xFF; // Get the second byte
  buffer[offset + 2] = (integer >> 8) & 0xFF;  // Get the second byte
  buffer[offset + 3] = integer & 0xFF;         // Get the second byte
}

static inline unsigned int combine_chars(char a, char b, char c, char d) {
  return ((unsigned int)(unsigned char)a << 24) |
         ((unsigned int)(unsigned char)b << 16) |
         ((unsigned int)(unsigned char)c << 8) |
         ((unsigned int)(unsigned char)d);
}

#endif // BIT_H
