#ifndef WAL_H_
#define WAL_H_

#include "wal_types.h"
#include "wal_frame.h"
#include "wal_io.h"
#include <stdbool.h>

/* =========================================================
 * WAL ITERATOR (ABSTRACTION LAYER)
 * ========================================================= */

typedef struct {
    WalLog *log;
    WalIoBuffer *io;
    uint32_t offset;
    uint32_t file_size;
} WalIterator;

typedef struct {
    const uint8_t *payload;
    uint32_t size;
    uint32_t offset;
    uint32_t frame_size;
} WalIterRecord;

int wal_append(WalLog *log,
               const void *payload,
               uint32_t size,
               uint8_t version,
               WalIoBuffer *io);

int wal_recover(WalLog *log, WalIoBuffer *io);

void wal_iter_init(WalIterator *it, WalLog *log, WalIoBuffer *io);

bool wal_iter_next(WalIterator *it, WalIterRecord *out);

#endif