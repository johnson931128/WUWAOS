#ifndef WUWAOS_PATH_H
#define WUWAOS_PATH_H
#include "path_limits.h"
int path_absolute(const char *cwd, const char *input, char output[WUWA_PATH_MAX]);
void path_canonical(const char *input, char output[WUWA_PATH_MAX]);
#endif
