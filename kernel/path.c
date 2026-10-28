#include "path.h"
#include <stddef.h>

/* Join without removing ..: every intermediate directory must exist. */
int path_absolute(const char *cwd, const char *input, char output[WUWA_PATH_MAX]) {
    size_t n = 0;
    if (!input || !*input) return -22;
    if (*input != '/') {
        while (*cwd) { if (n + 1 >= WUWA_PATH_MAX) return -36; output[n++] = *cwd++; }
        if (n > 1) { if (n + 1 >= WUWA_PATH_MAX) return -36; output[n++] = '/'; }
    }
    while (*input) { if (n + 1 >= WUWA_PATH_MAX) return -36; output[n++] = *input++; }
    output[n] = 0;
    return 0;
}

/* Only use after the original path has been resolved successfully. */
void path_canonical(const char *input, char output[WUWA_PATH_MAX]) {
    size_t n = 1; output[0] = '/';
    while (*input) {
        while (*input == '/') ++input;
        const char *start = input;
        while (*input && *input != '/') ++input;
        size_t length = (size_t)(input - start);
        if (!length || (length == 1 && start[0] == '.')) continue;
        if (length == 2 && start[0] == '.' && start[1] == '.') {
            while (n > 1 && output[n - 1] != '/') --n;
            if (n > 1) --n;
            continue;
        }
        if (n > 1) output[n++] = '/';
        for (size_t i = 0; i < length; ++i) output[n++] = start[i];
    }
    output[n] = 0;
}
