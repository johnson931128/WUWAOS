#ifndef WUWAOS_FS_H
#define WUWAOS_FS_H

#include <stddef.h>
#include <stdint.h>

/* Boot-only one-time format/mount/replay before scheduler startup; not a runtime remount API. */
int fs_init(void);
int fs_scope_enter(void);
int fs_scope_leave(void);
long fs_is_directory(const char *path);
long fs_list(const char *path, char *output, size_t capacity);
long fs_mkdir(const char *path);
long fs_touch(const char *path);
long fs_size(const char *path);
long fs_read(const char *path, size_t offset, void *buffer, size_t length);
long fs_write(const char *path, const void *buffer, size_t length);
long fs_remove(const char *path);
long fs_remove_tree(const char *path);
long fs_copy(const char *source, const char *destination);
long fs_open_inode(const char *path, int create, uint32_t *inode);
long fs_inode_size(uint32_t inode);
long fs_read_inode(uint32_t inode, size_t offset, void *buffer, size_t length);
long fs_write_inode(uint32_t inode, size_t offset, const void *buffer, size_t length);
long fs_truncate_inode(uint32_t inode);

#endif
