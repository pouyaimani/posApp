#ifndef EMBEDDBSETUP_H_
#define EMBEDDBSETUP_H_

#include <stdint.h>
#include "embedDB.h"
#include "embedDB_mem.h"
#include "embedDB/debug_print.h"
#include "EmbedDB_Utility/embedDBUtility.h"

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

#define PAGE_SIZE_512       512
#define PAGE_SIZE_1024      1024
#define PAGE_SIZE_2048      2048
#define PAGE_SIZE_4096      4096

int8_t embedDBSetup(embedDBState *state,
                    const char *dbPath,
                    const char *dbIndexPath,
                    uint16_t keySize,
                    uint16_t dataSize,
                    uint32_t pageSize,
                    uint16_t pageNum);
int8_t embedDBtearDown(embedDBState *state);

int8_t embedDBreset(embedDBState *state,
                    const char *dbPath,
                    const char *dbIndexPath,
                    uint16_t keySize,
                    uint16_t dataSize,
                    uint32_t pageSize,
                    uint16_t pageNum);

#endif