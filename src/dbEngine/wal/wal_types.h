#ifndef WAL_TYPES_H_
#define WAL_TYPES_H_

#include <stdint.h>

/* ================= WAL CONTEXT ================= */

typedef struct WalOps {
    int (*read)(void *ctx, uint8_t *buf, uint32_t size, uint32_t offset);
    int (*write)(void *ctx, const uint8_t *buf, uint32_t size, uint32_t offset);
    int (*sync)(void *ctx);
    uint32_t (*size)(void *ctx);
} WalOps;

/* ================= WAL HANDLE ================= */

typedef struct WalLog {
    WalOps ops;
    void *ctx;
    uint32_t write_offset;
} WalLog;

/* ================= IO BUFFER ================= */

typedef struct {
    uint8_t *buf;
    uint32_t size;
} WalIoBuffer;

#endif