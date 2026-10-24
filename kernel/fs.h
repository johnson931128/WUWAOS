#ifndef WUWAOS_FS_H
#define WUWAOS_FS_H

#include <stddef.h>

int fs_init(void);
long fs_list(const char *path, char *output, size_t capacity);
long fs_mkdir(const char *path);
long fs_touch(const char *path);
long fs_read(const char *path, size_t offset, void *buffer, size_t length);
long fs_write(const char *path, const void *buffer, size_t length);

#endif
