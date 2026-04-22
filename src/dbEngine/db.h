#ifndef DB_H_
#define DB_H_

#include "wal/wal.h"
#include "indexing.h"
#include "qEngine.h"

typedef struct {
    const uint8_t *data;
    uint32_t size;
} DbDoc;

typedef uint32_t (*DbKeyFn)(const uint8_t *doc, uint32_t size);
typedef uint8_t  (*DbFilterFn)(const uint8_t *doc, uint32_t size, void *ctx);

typedef struct {
    WalLog *wal;
    WalIoBuffer *io;
    IndexDB *index;

    DbKeyFn key_fn;
} GenericDB;

#endif