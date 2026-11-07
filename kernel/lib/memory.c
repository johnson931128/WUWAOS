#include <stddef.h>

void *memset(void *destination, int value, size_t length) {
    unsigned char *bytes = destination;
    for (size_t i = 0; i < length; ++i) bytes[i] = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t length) {
    unsigned char *to = destination;
    const unsigned char *from = source;
    for (size_t i = 0; i < length; ++i) to[i] = from[i];
    return destination;
}
