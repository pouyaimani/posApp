#ifndef QENGINE_H_
#define QENGINE_H_

#include <stdint.h>
#include <stdbool.h>
#include "wal/wal.h"
#include "indexing.h"

/* =========================================================
 * RESULT MODEL
 * ========================================================= */

typedef struct {
    uint32_t *offsets;
    uint32_t count;
} QueryResult;

/* =========================================================
 * QUERY ENGINE CONTEXT
 * ========================================================= */

typedef struct {
    IndexDB *index;
    WalLog  *wal;
    WalIoBuffer *io;
} QueryEngine;

typedef struct {
    uint8_t use_key;
    uint32_t key;

    uint8_t use_status;
    uint8_t status;

    uint8_t use_time;
    uint8_t time_prefix[4];

    uint8_t fallback_scan;
} QueryPlan;

int qe_execute(QueryEngine *qe,
               QueryPlan *plan,
               QueryResult *res);

#endif