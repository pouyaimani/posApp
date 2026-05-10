#include "txnRecs.h"
#include "embedDB/embedDB.h"
#include "embedDB/embedDBsetup.h"
#include "embedDB/embedDB_mem.h"
#include "logger.h"
#include "utility/utility.h"

static TxnRecord __txnrecord;
static TxnQuery __txnquery;

#define TRANS_RECORD_PATH       "/mtd0/txn_records.bin"
#define TRANS_IDX_PATH          "/mtd0/txn_idx.bin"

#define PAGE_NUMBER             10

#define MAX_RECORDS 6000

static embedDBState *state;
static embedDBSchema *schema;

static int txnInsert(TxnData *txn) {
    if (!txn || !state) return ERR_NOK;
    LOG_DEBUG("time stamp = %llu", txn->dateTime);
    uint32_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    if (embedDBPut(state, &txn->dateTime, txn) != 0) {
        LOG_ERROR("Transaction record: error in inserting record.");
        return -1;
    }
    LOG_DEBUG("txn: date = %lu, time = %lu, trace = %s, stan = %s, rrn = %s, amount = %s",
                date, time, txn->trace, txn->stan, txn->RRN, txn->amount);
    return 0;
}

static int8_t iterateThrough() {
    if (!state) return ERR_NOK;
    embedDBIterator it;
    embedDBInitIterator(state, &it);
    uint64_t timeStamp;
    TxnData data;
    while (embedDBNext(state, &it, &timeStamp, &data)) {
        uint32_t date, time;
        unpackDateTime(data.dateTime, &date, &time);
        LOG_DEBUG("txn: date = %lu, time = %lu, trace = %s, stan = %s, rrn = %s, amount = %s",
                 date, time, data.trace, data.stan, data.RRN, data.amount);
    }
    return ERR_OK;
}

static int8_t txnReset() {
    if (!state) return ERR_NOK;
    if (embedDBreset(state, TRANS_RECORD_PATH, TRANS_IDX_PATH) != 0) {
                LOG_ERROR("Error in reseting embedDB.");
                return ERR_NOK;
    }
    return ERR_OK;
}

static int8_t queryInit(QueryOperator *qo) {
    if (!qo) {
        LOG_ERROR("Txn record: QueryOperator is NULL.");
        return ERR_NOK;
    }
    qo->it = (embedDBIterator*)GET_MEM(sizeof(embedDBIterator));
    if (!qo->it) {
        LOG_ERROR("Txn record: Failed to allocate iterator.");
        return ERR_NOK;
    }
    memset(qo->it, 0, sizeof(embedDBIterator));
    embedDBInitIterator(state, qo->it);
    qo->op = createTableScanOperator(state, qo->it, schema);
    if (!(qo->op)) {
        LOG_ERROR("Failed to create table scan operator.");
        embedDBCloseIterator(qo->it);
        EMDB_MEM_FREE(qo->it);
        return ERR_NOK;
    }
    return ERR_OK;
}

static void queryWhere(QueryOperator *qo,
                        int column,
                        int comparison,
                        void *value) {
    if (!qo || !value)
        return;
    embedDBOperator *newOp =
        createSelectionOperator(qo->op, column, comparison, value);

    if (!newOp) {
        LOG_ERROR("Txn query: creating selection operator failed.");
        return;
    }
    qo->op = newOp;
}

static void txnSelect(QueryOperator *qo, TxnHandler handler) {
    if (!qo || !handler)
        return;
    (qo->op)->init(qo->op);
    while (exec(qo->op)) {
        TxnData data;
        uint8_t *buf = (uint8_t *)qo->op->recordBuffer;
        memcpy(&data, buf + state->keySize, sizeof(TxnData));
        if (!handler(&data, NULL))
            break;
    }
    embedDBCloseIterator(qo->it);
    (qo->op)->close((qo->op));
    EMDB_MEM_FREE(qo->it);
    embedDBFreeOperatorRecursive(qo->op);
}

static int8_t init(TxnRecord *self) {
    state = (embedDBState *)EMDB_MEM_ALLOC(sizeof(embedDBState));
    if (!state) {
        LOG_ERROR("Transaction records: not enough memory for embedDB.");
        return -1;
    }
    int8_t colSizes[] = {
        state->keySize,                                                 // key
        sizeof(sizeof(((TxnData*)0)->id)),                              // id
        sizeof(sizeof(((TxnData*)0)->processCode)),                     // processCode
        sizeof(sizeof(((TxnData*)0)->maskedPan)),                       // maskedPan
        sizeof(sizeof(((TxnData*)0)->purchaseId)),                      // purchaseId
        sizeof(sizeof(((TxnData*)0)->amount)),                          // amount
        sizeof(sizeof(((TxnData*)0)->priceWithDiscount)),               // priceWithDiscount
        sizeof(sizeof(((TxnData*)0)->stan)),                            // stan
        sizeof(sizeof(((TxnData*)0)->trace)),                           // trace
        sizeof(sizeof(((TxnData*)0)->dateTime)),                        // dateTime
        sizeof(sizeof(((TxnData*)0)->RRN)),                             // RRN
        sizeof(sizeof(((TxnData*)0)->billId)),                          // billId
        sizeof(sizeof(((TxnData*)0)->paymentId)),                       // paymentId
        sizeof(sizeof(((TxnData*)0)->companyId)),                       // companyId
        sizeof(sizeof(((TxnData*)0)->companyName)),                     // companyName
        sizeof(sizeof(((TxnData*)0)->phoneNumber)),                     // phoneNumber
        sizeof(sizeof(((TxnData*)0)->chargeLevel)),                     // chargeLevel
        sizeof(sizeof(((TxnData*)0)->accountIndex)),                    // accountIndex
        sizeof(sizeof(((TxnData*)0)->accountCaption)),                  // accountCaption
        sizeof(sizeof(((TxnData*)0)->responseCode)),                    // responseCode
        sizeof(sizeof(((TxnData*)0)->Status))                           // Status
    };

    int8_t colSignedness[] = {
        embedDB_COLUMN_UNSIGNED, // key
        embedDB_COLUMN_UNSIGNED, // id
        embedDB_COLUMN_UNSIGNED, // processCode
        embedDB_COLUMN_UNSIGNED, // maskedPan
        embedDB_COLUMN_UNSIGNED, // purchaseId
        embedDB_COLUMN_UNSIGNED, // amount
        embedDB_COLUMN_UNSIGNED, // priceWithDiscount
        embedDB_COLUMN_UNSIGNED, // stan
        embedDB_COLUMN_UNSIGNED, // trace
        embedDB_COLUMN_UNSIGNED, // dateTime
        embedDB_COLUMN_UNSIGNED, // RRN
        embedDB_COLUMN_UNSIGNED, // billId
        embedDB_COLUMN_UNSIGNED, // paymentId
        embedDB_COLUMN_UNSIGNED, // companyId
        embedDB_COLUMN_UNSIGNED, // companyName
        embedDB_COLUMN_UNSIGNED, // phoneNumber
        embedDB_COLUMN_UNSIGNED, // chargeLevel
        embedDB_COLUMN_UNSIGNED, // accountIndex
        embedDB_COLUMN_UNSIGNED, // accountCaption
        embedDB_COLUMN_UNSIGNED, // responseCode
        embedDB_COLUMN_SIGNED    // Status
    };
    ColumnType colTypes[] = {
        embedDB_COLUMN_UINT64,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT64,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_UINT32,
        embedDB_COLUMN_INT32
    };

    uint32_t parameters = EMBEDDB_RECORD_LEVEL_CONSISTENCY
                            | (EMBEDDB_USE_BMAP | EMBEDDB_USE_INDEX);
    LOG_ERROR("size of txnData = %u.", sizeof(TxnData));                  
    if (embedDBSetup(state, TRANS_RECORD_PATH, TRANS_IDX_PATH, sizeof(uint64_t),
             sizeof(TxnData), PAGE_SIZE_512, PAGE_NUMBER, parameters) != 0) {
                embedDBClose(state);
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                state = NULL;
                LOG_ERROR("Error in setuping embedDB.");
                return ERR_NOK;
    }

    schema = embedDBCreateSchema(21, colSizes, colSignedness, colTypes);
    if (!schema) {
        LOG_ERROR("Txn query error: failed to create schema.");
        embedDBClose(state);
        embedDBtearDown(state);
        EMDB_MEM_FREE(state);
        state = NULL;
        return ERR_NOK;
    }
    return ERR_OK;
}

OOP_CTOR(TxnRecord) {
    self->init = init;
    self->insert = txnInsert;
    self->select = txnSelect;
    self->reset = txnReset;
    self->iterate = iterateThrough;
}

TxnRecord *txnrecord() {
    CALL_ONCE(
        OOP_CALL_CTOR(TxnRecord, &__txnrecord);
    );
    return &__txnrecord;
}

OOP_CTOR(TxnQuery) {
    self->init = queryInit;
    self->where = queryWhere;
}

TxnQuery *txnquery() {
    CALL_ONCE(
        OOP_CALL_CTOR(TxnQuery, &__txnquery);
    );
    return &__txnquery;
}