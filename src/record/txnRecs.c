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

static Result_t txnInsert(TxnData* txn) {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(txn, res.err = ERR_DSC_INVALID_ARG, res);
    RETURN_VALUE_IF_NULL(state, res.err = ERR_DSC_INVALID_ARG, res);
    LOG_DEBUG("time stamp = %llu", txn->dateTime);
    uint32_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    TxnRec_t rec;
    rec.core      = txn->core;
    rec.extention = txn->extention;
    if (embedDBPut(state, &txn->dateTime, &rec) != 0) {
        LOG_ERROR("Transaction record: error in inserting record.");
        res.err       = ERR_DSC_DATABASE;
        res.detail.db = DB_ERR_INSERT_FAILURE;
        return res;
    }
    LOG_DEBUG(
        "txn: date = %lu, time = %lu, trace = %s, refNum = %s, stan = %s, "
        "amount = %s",
        date, time, txn->core.trace, txn->core.rrn, txn->core.stan,
        txn->core.amount);
    return res;
}

static Result_t iterateThrough() {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(state, res.err = ERR_DSC_INVALID_ARG, res);
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
            date, time, rec.core.trace, rec.core.rrn, rec.core.stan,
            rec.core.amount);
    }
    return res;
}

static Result_t txnReset() {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(state, res.err = ERR_DSC_INVALID_ARG, res);
    if (embedDBreset(state, TRANS_RECORD_PATH, TRANS_IDX_PATH) != 0) {
        LOG_ERROR("Error in reseting embedDB.");
        res.err       = ERR_DSC_DATABASE;
        res.detail.db = DB_ERR_RESET_FAILURE;
        return res;
    }
    return res;
}

static Result_t queryInit(QueryOperator* qo) {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    qo->it = (embedDBIterator*)MEM_ALLOC(sizeof(embedDBIterator));
    RETURN_VALUE_IF_NULL(qo->it, res.err = ERR_DSC_MEMORY, res);
    memset(qo->it, 0, sizeof(embedDBIterator));
    embedDBInitIterator(state, qo->it);
    qo->op = createTableScanOperator(state, qo->it, schema);
    if (!(qo->op)) {
        LOG_ERROR("Failed to create table scan operator.");
        embedDBCloseIterator(qo->it);
        EMDB_MEM_FREE(qo->it);
        res.err       = ERR_DSC_DATABASE;
        res.detail.db = DB_ERR_CREATING_OP_FAILURE;
        return res;
    }
    return res;
}

static Result_t queryWhere(QueryOperator* qo, int column, int comparison,
                           void* value) {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    RETURN_VALUE_IF_NULL(value, res.err = ERR_DSC_INVALID_ARG, res);
    embedDBOperator* newOp =
        createSelectionOperator(qo->op, column, comparison, value);
    RETURN_VALUE_IF_NULL(newOp, res.err = ERR_DSC_MEMORY, res);
    qo->op = newOp;
}

static Result_t txnSelect(QueryOperator* qo, TxnHandler handler,
                          void* userData) {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    RETURN_VALUE_IF_NULL(handler, res.err = ERR_DSC_INVALID_ARG, res);
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
    return res;
}

static Result_t init(TxnRecord* self) {
    Result_t res = {.err = ERR_DSC_OK};
    state        = (embedDBState*)EMDB_MEM_ALLOC(sizeof(embedDBState));
    RETURN_VALUE_IF_NULL(state, res.err = ERR_DSC_MEMORY, res);

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
        res.err       = ERR_DSC_DATABASE;
        res.detail.db = DB_ERR_SETUP_FAILURE;
        return res;
    }

    int8_t colSizes[] = {
        state->keySize,                                  // key
        sizeof(sizeof(((TxnData*)0)->core.txnType)),     // type
        sizeof(sizeof(((TxnData*)0)->core.processCode)), // processCode
        sizeof(sizeof(((TxnData*)0)->core.pan)),         // maskedPan
        sizeof(sizeof(((TxnData*)0)->core.amount)),      // amount
        sizeof(sizeof(((TxnData*)0)->core.rrn)),         // refNum
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

    schema = embedDBCreateSchema((sizeof(colSizes) / colSizes[0]), colSizes,
                                 colSignedness, colTypes);
    if (!schema) {
        LOG_ERROR("Txn query error: failed to create schema.");
        embedDBClose(state);
        embedDBtearDown(state);
        EMDB_MEM_FREE(state);
        state         = NULL;
        res.err       = ERR_DSC_DATABASE;
        res.detail.db = DB_ERR_CREATING_SCHEMA_FAILURE;
        return res;
    }
    return res;
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