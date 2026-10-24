#include "api.h"
#include "print.h"

int user_write_all(long fd, const char *text, unsigned long length) {
    unsigned long offset = 0;
    while (offset < length) {
        const long written = write(fd, text + offset, length - offset);
        if (written <= 0) {
            return -1;
        }
        offset += (unsigned long)written;
    }
    return 0;
}

void user_puts(const char *text) {
    unsigned long length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    if (user_write_all(1, text, length) < 0) {
        exit(90);
    }
}

void user_number(long value) {
    char digits[21];
    unsigned long number = (unsigned long)value;
    unsigned long length = 0;
    if (value < 0) {
        user_puts("-");
        number = 0UL - number;
    }
    do {
        digits[length++] = (char)('0' + number % 10);
        number /= 10;
    } while (number != 0);
    for (unsigned long i = 0; i < length / 2; ++i) {
        const char temporary = digits[i];
        digits[i] = digits[length - 1 - i];
        digits[length - 1 - i] = temporary;
    }
    user_write_all(1, digits, length);
}
