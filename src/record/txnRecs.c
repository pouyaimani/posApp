#include "txnRecs.h"
#include "embedDB/embedDB.h"
#include "embedDB/embedDBsetup.h"
#include "embedDB/embedDB_mem.h"
#include "logger.h"
#include "utility/utility.h"
#include "common.h"

typedef struct {
    TxnCore      core;
    TxnExtention extention;
} TxnRec_t;

static TxnRecord __txnrecord;
static TxnQuery  __txnquery;

#define TRANS_RECORD_PATH "/mtd0/txn_records.bin"
#define TRANS_IDX_PATH    "/mtd0/txn_idx.bin"

#define PAGE_NUMBER 10

#define MAX_RECORDS 6000

static embedDBState*  state;
static embedDBSchema* schema;

static int txnInsert(TxnData* txn) {
    if (!txn || !state)
        return ERR_NOK;
    LOG_DEBUG("time stamp = %llu", txn->dateTime);
    uint32_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    TxnRec_t rec;
    rec.core      = txn->core;
    rec.extention = txn->extention;
    if (embedDBPut(state, &txn->dateTime, &rec) != 0) {
        LOG_ERROR("Transaction record: error in inserting record.");
        return -1;
    }
    LOG_DEBUG(
        "txn: date = %lu, time = %lu, trace = %s, refNum = %s, stan = %s, "
        "amount = %s",
        date, time, txn->core.trace, txn->core.refNum, txn->core.stan,
        txn->core.amount);
    return 0;
}

static int8_t iterateThrough() {
    if (!state)
        return ERR_NOK;
    embedDBIterator it;
    embedDBInitIterator(state, &it);
    uint64_t timeStamp;
    TxnRec_t rec;
    while (embedDBNext(state, &it, &timeStamp, &rec)) {
        uint32_t date, time;
        unpackDateTime(timeStamp, &date, &time);
        LOG_DEBUG(
            "txn: date = %lu, time = %lu, trace = %s, refNum = %s, stan = "
            "%s, amount = %s",
            date, time, rec.core.trace, rec.core.refNum, rec.core.stan,
            rec.core.amount);
    }
    return ERR_OK;
}

static int8_t txnReset() {
    if (!state)
        return ERR_NOK;
    if (embedDBreset(state, TRANS_RECORD_PATH, TRANS_IDX_PATH) != 0) {
        LOG_ERROR("Error in reseting embedDB.");
        return ERR_NOK;
    }
    return ERR_OK;
}

static int8_t queryInit(QueryOperator* qo) {
    if (!qo) {
        LOG_ERROR("Txn record: QueryOperator is NULL.");
        return ERR_NOK;
    }
    qo->it = (embedDBIterator*)MEM_ALLOC(sizeof(embedDBIterator));
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

static void queryWhere(QueryOperator* qo, int column, int comparison,
                       void* value) {
    if (!qo || !value)
        return;
    embedDBOperator* newOp =
        createSelectionOperator(qo->op, column, comparison, value);

    if (!newOp) {
        LOG_ERROR("Txn query: creating selection operator failed.");
        return;
    }
    qo->op = newOp;
}

static void txnSelect(QueryOperator* qo, TxnHandler handler, void* userData) {
    if (!qo || !handler)
        return;
    (qo->op)->init(qo->op);
    while (exec(qo->op)) {
        TxnData  data;
        uint8_t* buf = (uint8_t*)qo->op->recordBuffer;
        memcpy(&data, buf + state->keySize, sizeof(TxnData));
        if (!handler(&data, userData))
            break;
    }
    embedDBCloseIterator(qo->it);
    (qo->op)->close((qo->op));
    EMDB_MEM_FREE(qo->it);
    embedDBFreeOperatorRecursive(qo->op);
}

static int8_t init(TxnRecord* self) {
    state = (embedDBState*)EMDB_MEM_ALLOC(sizeof(embedDBState));
    if (!state) {
        LOG_ERROR("Transaction records: not enough memory for embedDB.");
        return -1;
    }
    int8_t colSizes[] = {
        state->keySize,                                  // key
        sizeof(sizeof(((TxnData*)0)->core.txnType)),     // type
        sizeof(sizeof(((TxnData*)0)->core.processCode)), // processCode
        sizeof(sizeof(((TxnData*)0)->core.pan)),         // maskedPan
        sizeof(sizeof(((TxnData*)0)->core.amount)),      // amount
        sizeof(sizeof(((TxnData*)0)->core.refNum)),      // refNum
        sizeof(sizeof(((TxnData*)0)->core.trace)),       // trace
        sizeof(sizeof(((TxnData*)0)->core.stan)),        // RRN
        sizeof(sizeof(((TxnData*)0)->core.respCode)),    // responseCode
        sizeof(sizeof(((TxnData*)0)->extention))         // extention
    };

    int8_t colSignedness[] = {
        embedDB_COLUMN_UNSIGNED, // key
        embedDB_COLUMN_UNSIGNED, // id
        embedDB_COLUMN_UNSIGNED, // processCode
        embedDB_COLUMN_UNSIGNED, // maskedPan
        embedDB_COLUMN_UNSIGNED, // amount
        embedDB_COLUMN_UNSIGNED, // refNum
        embedDB_COLUMN_UNSIGNED, // trace
        embedDB_COLUMN_UNSIGNED, // RRN
        embedDB_COLUMN_UNSIGNED, // responseCode
        embedDB_COLUMN_UNSIGNED  // extention
    };
    ColumnType colTypes[] = {embedDB_COLUMN_UINT64, embedDB_COLUMN_UINT32,
                             embedDB_COLUMN_UINT32, embedDB_COLUMN_UINT32,
                             embedDB_COLUMN_UINT32, embedDB_COLUMN_UINT32,
                             embedDB_COLUMN_UINT32, embedDB_COLUMN_UINT32,
                             embedDB_COLUMN_UINT32, embedDB_COLUMN_UINT32};

    uint32_t parameters = EMBEDDB_RECORD_LEVEL_CONSISTENCY |
                          (EMBEDDB_USE_BMAP | EMBEDDB_USE_INDEX);
    LOG_ERROR("size of txnData = %u.", sizeof(TxnRec_t));
    if (embedDBSetup(state, TRANS_RECORD_PATH, TRANS_IDX_PATH, sizeof(uint64_t),
                     sizeof(TxnRec_t), PAGE_SIZE_512, PAGE_NUMBER,
                     parameters) != 0) {
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
    self->init    = init;
    self->insert  = txnInsert;
    self->select  = txnSelect;
    self->reset   = txnReset;
    self->iterate = iterateThrough;
}

TxnRecord* txnrecord() {
    CALL_ONCE(OOP_CALL_CTOR(TxnRecord, &__txnrecord););
    return &__txnrecord;
}

OOP_CTOR(TxnQuery) {
    self->init  = queryInit;
    self->where = queryWhere;
}

TxnQuery* txnquery() {
    CALL_ONCE(OOP_CALL_CTOR(TxnQuery, &__txnquery););
    return &__txnquery;
}