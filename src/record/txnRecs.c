#include "txnRecs.h"
#include "embedDB/embedDB.h"
#include "embedDB/embedDBsetup.h"
#include "embedDB/embedDB_mem.h"
#include "logger.h"
#include "file/file.h"

static TxnRecord __txnrecord;

#define TRANS_RECORD_PATH       "/mtd0/txn_records.bin"
#define TRANS_IDX_PATH          "/mtd0/txn_idx.bin"

#define PAGE_NUMBER             1

#define MAX_RECORDS 6000

static embedDBState *state;

static int txnInsert(TxnData_t *txn) {
    if (embedDBSetup(state, TRANS_RECORD_PATH, TRANS_IDX_PATH, sizeof(uint32_t),
             sizeof(TxnData_t), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    // int8_t res = embedDBPut(state, &latestIdx, txn);
    // if (res != 0) {
    //     LOG_ERROR("Shift: error in inserting record. error = %d", res);
    //     return -1;
    // }
    // LOG_DEBUG("shift: idx = %d, startTime = %d, endTime = %d, startDate = %d, endDate = %d",
    //         latestIdx, shift->startTime, shift->endTime, shift->startDate, shift->endDate);
    embedDBClose(state);
    embedDBtearDown(state);
    return 0;
}

/* ================= OOP ================= */

OOP_CTOR(TxnRecord) {
    // self->init = recordInit;
    // self->add = recordAdd;
    // self->getLastIdx = recordGetLastIndex;
    // // self->count = recordCount;

    state = (embedDBState *)EMDB_MEM_ALLOC(sizeof(embedDBState));
    if (!state) {
        LOG_ERROR("Transaction records: not enough memory for embedDB.");
        return -1;
    }
}

TxnRecord *txnrecord() {
    CALL_ONCE(
        OOP_CALL_CTOR(TxnRecord, &__txnrecord);
    );
    return &__txnrecord;
}