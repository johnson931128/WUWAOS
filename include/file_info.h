#ifndef WUWAOS_FILE_INFO_H
#define WUWAOS_FILE_INFO_H

#include <stdint.h>

#define WUWA_FILE_DIRECTORY 1U
#define WUWA_FILE_REGULAR 2U

typedef struct {
    uint64_t size;
    uint32_t inode;
    uint32_t type;
    uint32_t links;
    uint32_t reserved;
} WuwaFileInfo;

typedef struct {
    uint32_t inode;
    char name[24];
} WuwaDirEntry;

#endif
