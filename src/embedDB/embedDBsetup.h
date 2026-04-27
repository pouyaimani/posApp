#ifndef EMBEDDBSETUP_H_
#define EMBEDDBSETUP_H_

#include <stdint.h>
#include "embedDB.h"

#define EMBEDDB_TUNE_FINE       8
#define EMBEDDB_TUNE_BALANCED   16
#define EMBEDDB_TUNE_THROUGHPUT 32

#ifndef EMBEDDB_RPP_TARGET
#define EMBEDDB_RPP_TARGET EMBEDDB_TUNE_BALANCED
#endif

#if (EMBEDDB_RPP_TARGET != 8) && \
    (EMBEDDB_RPP_TARGET != 16) && \
    (EMBEDDB_RPP_TARGET != 32)
#error "Invalid EMBEDDB_RPP_TARGET. Use 8, 16, or 32."
#endif

#define EMBEDDB_IS_GOOD_RPP(rpp) ((rpp) >= EMBEDDB_RPP_TARGET)

void setupEmbedDB(embedDBState *state, const char *dbName, uint32_t maxStorageBytes,
                                  uint32_t recordSize, uint32_t keySize);
#endif