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

#define PAGE_NUMBER             1

#define MAX_RECORDS 6000

static embedDBState *state;
static embedDBSchema *schema;

static int txnInsert(TxnData_t *txn) {
    if (embedDBSetup(state, TRANS_RECORD_PATH, TRANS_IDX_PATH, sizeof(TxnIndex_t),
             sizeof(TxnData_t), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    uint32_t date, time;
    extractDatetimeInt(txn->dateTime, &date, &time);
    long long timeStamp = packDateTime(date, time);
    if (embedDBPut(state, &timeStamp, txn) != 0) {
        LOG_ERROR("Transaction record: error in inserting record.");
        return -1;
    }
    // LOG_DEBUG("shift: idx = %d, startTime = %d, endTime = %d, startDate = %d, endDate = %d",
    //         latestIdx, shift->startTime, shift->endTime, shift->startDate, shift->endDate);
    embedDBClose(state);
    embedDBtearDown(state);
    return 0;
}

static int8_t txnReset() {
    if (embedDBreset(state, TRANS_RECORD_PATH, TRANS_IDX_PATH, sizeof(TxnIndex_t),
             sizeof(TxnData_t), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                LOG_ERROR("Error in setuping embedDB.");
                return ERR_NOK;
    }
    return ERR_OK;
}

static QueryOperation_t queryInit(QueryOperation_t *qo) {
    qo->it = GET_MEM(sizeof(embedDBIterator));
    embedDBInitIterator(state, &qo->it);
    QueryOperation_t qop;
    qop.op = createTableScanOperator(state, &qo->it, schema);
    return qop;
}

static void queryWhere(QueryOperation_t *qo, QueryColumn_t culomn, int comparison, void *value) {
    QueryOperation_t op;
    op.op = createSelectionOperator(qo->op, culomn, comparison, value);
    return op;
}

static void txnSelect(QueryOperation_t *qo, TxnHandler handler) {
    QueryOperation_t op;
        uint16_t Col[] = {
        0,  // id
        1,  // processCode
        2, // maskedPan
        3, // purchaseId
        4, // amount
        5, // priceWithDiscount
        6,  // stan
        7,  // trace
        8, // dateTime
        9, // RRN
        10, // billId
        11, // paymentId
        12,  // companyId
        13, // companyName
        14, // phoneNumber
        15,  // chargeLevel
        16,  // accountIndex
        17, // accountCaption
        18,  // responseCode
        19   // Status
    };
    op.op = createProjectionOperator(qo->op, 20, Col);
    op.op->init(op.op);
    int32_t recordsReturned = 0;
    while (exec(op.op)) {
        recordsReturned++;
        TxnData_t data;
        memcpy(&data, op.op->recordBuffer, sizeof(TxnData_t));
        handler(&data, NULL);
    }

    op.op->close(op.op);
    embedDBFreeOperatorRecursive(&op.op);
}

OOP_CTOR(TxnRecord) {
    self->insert = txnInsert;
    self->select = txnSelect;
    self->reset = txnReset;

    state = (embedDBState *)EMDB_MEM_ALLOC(sizeof(embedDBState));
    if (!state) {
        LOG_ERROR("Transaction records: not enough memory for embedDB.");
        return -1;
    }
    uint16_t colSizes[] = {
        1,  // id
        7,  // processCode
        17, // maskedPan
        31, // purchaseId
        13, // amount
        13, // priceWithDiscount
        7,  // stan
        7,  // trace
        15, // dateTime
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
        4   // Status
    };

    int8_t colSignedness[] = {
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

    schema = embedDBCreateSchema(20, colSizes, colSignedness, colTypes);
}

TxnRecord *txnrecord() {
    CALL_ONCE(
        OOP_CALL_CTOR(TxnRecord, &__txnrecord);
    );
    return &__txnrecord;
}

OOP_CTOR(TxnQuery) {
    self->init = queryInit;
    self->where = queryInit;
}

TxnQuery *txnquery() {
    CALL_ONCE(
        OOP_CALL_CTOR(TxnQuery, &__txnquery);
    );
    return &__txnquery;
}