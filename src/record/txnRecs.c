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
    qo->op = createSelectionOperator(qo->op, column, comparison, value);
}

static void txnSelect(QueryOperator *qo, TxnHandler handler) {
    if (!qo || !handler)
        return;
    (qo->op)->init(qo->op);
    while (exec(qo->op)) {
        TxnData data;
        memcpy(&data, (qo->op)->recordBuffer + state->keySize, sizeof(TxnData));
        handler(&data, NULL);
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
        8, // key
        1,  // id
        7,  // processCode
        17, // maskedPan
        31, // purchaseId
        13, // amount
        13, // priceWithDiscount
        7,  // stan
        7,  // trace
        8, // dateTime
        13, // RRN
        24, // billId
        24, // paymentId
        8,  // companyId
        64, // companyName
        12, // phoneNumber
        1,  // chargeLevel
        8,  // accountIndex
        33, // accountCaption
        3,  // responseCode
        2   // Status
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