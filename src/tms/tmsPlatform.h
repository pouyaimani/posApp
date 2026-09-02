#ifndef TMS_PLATFORM_H
#define TMS_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int (*removeFile)(const char *path);
    int (*createFile)(const char *path, uint32_t size);
    int (*readFile)(const char *path, uint32_t offset, uint8_t *data,
                    size_t requested, size_t *actual);
    int (*writeFile)(const char *path, uint32_t offset, const uint8_t *data,
                     size_t length);
    uint32_t (*fileSize)(const char *path);
    int (*syncFiles)(void);
    int (*setUpdateFlag)(void);
    void (*reboot)(void);
} TmsPlatformOps;

#endif
