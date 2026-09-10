#include "txnRecs.h"
#include "embedDB/embedDB.h"
#include "embedDB/embedDBsetup.h"
#include "embedDB/embedDB_mem.h"
#include "logger.h"
#include "utility/utility.h"
#include "common.h"

/**
 * @brief Path to the transaction record data file.
 */
#define TRANS_RECORD_PATH "/mtd0/txn_records.bin"

/**
 * @brief Path to the transaction database index file.
 */
#define TRANS_IDX_PATH "/mtd0/txn_idx.bin"

/**
 * @brief Number of pages allocated to the transaction database.
 */
#define PAGE_NUMBER 10

/**
 * @brief Maximum number of transaction records supported by the database.
 */
#define MAX_RECORDS 6000

/**
 * @brief Serialized size of a transaction record excluding its database key.
 *
 * This value is calculated from the individual TxnData members instead of
 * using sizeof(TxnData), because TxnData may contain fields that are not
 * persisted and may also contain compiler-dependent padding.
 */
#define TXN_RECORD_SIZE                                                        \
    (sizeof(((TxnData*)0)->status) + sizeof(((TxnData*)0)->core.txnType) +     \
     sizeof(((TxnData*)0)->core.mti) +                                         \
     sizeof(((TxnData*)0)->core.processCode) +                                 \
     sizeof(((TxnData*)0)->core.pan) + sizeof(((TxnData*)0)->core.amount) +    \
     sizeof(((TxnData*)0)->core.rrn) + sizeof(((TxnData*)0)->core.trace) +     \
     sizeof(((TxnData*)0)->core.stan) + sizeof(((TxnData*)0)->core.respCode) + \
     sizeof(((TxnData*)0)->extention))

/**
 * @brief Global embedDB database state.
 *
 * Created by init() and shared by transaction record operations.
 */
static embedDBState* state = NULL;

/**
 * @brief Schema describing the serialized transaction record layout.
 *
 * Created during transaction database initialization and used by query
 * operators.
 */
static embedDBSchema* schema = NULL;

/**
 * @brief Singleton instance of the transaction record service.
 */
static TxnRecord __txnrecord;

/**
 * @brief Singleton instance of the transaction query service.
 */
static TxnQuery __txnquery;

/**
 * @brief Temporary key buffer used for limited transaction queries.
 *
 * The keys are maintained in sorted order by insertKey().
 *
 * For QUERY_LIMIT_EARLIEST, keys are stored in ascending order.
 * For QUERY_LIMIT_LATEST, keys are stored in descending order.
 */
static uint64_t sortedkeys[10];

/**
 * @brief Serialize a transaction into its database record representation.
 *
 * Only the persistent fields of TxnData are copied. The database key
 * (dateTime) is stored separately by embedDB and is therefore not included
 * in the serialized record.
 *
 * @param[out] buf Destination buffer.
 * @param[in] txn Transaction to serialize.
 */
static void serialize(uint8_t* buf, TxnData* txn) {
    uint8_t* p = buf;

    memcpy(p, &txn->status, sizeof(txn->status));
    p += sizeof(txn->status);

    memcpy(p, &txn->core.txnType, sizeof(txn->core.txnType));
    p += sizeof(txn->core.txnType);

    memcpy(p, &txn->core.mti, sizeof(txn->core.mti));
    p += sizeof(txn->core.mti);

    memcpy(p, &txn->core.processCode, sizeof(txn->core.processCode));
    p += sizeof(txn->core.processCode);

    memcpy(p, txn->core.pan, sizeof(txn->core.pan));
    p += sizeof(txn->core.pan);

    memcpy(p, &txn->core.amount, sizeof(txn->core.amount));
    p += sizeof(txn->core.amount);

    memcpy(p, &txn->core.rrn, sizeof(txn->core.rrn));
    p += sizeof(txn->core.rrn);

    memcpy(p, &txn->core.trace, sizeof(txn->core.trace));
    p += sizeof(txn->core.trace);

    memcpy(p, &txn->core.stan, sizeof(txn->core.stan));
    p += sizeof(txn->core.stan);

    memcpy(p, &txn->core.respCode, sizeof(txn->core.respCode));
    p += sizeof(txn->core.respCode);

    memcpy(p, &txn->extention, sizeof(txn->extention));
}

/**
 * @brief Deserialize a database record into a TxnData structure.
 *
 * The supplied key is assigned to txn->dateTime and the serialized
 * transaction fields are restored from the record buffer.
 *
 * @param[in]  buf  Serialized transaction record.
 * @param[in]  key  Database key associated with the record.
 * @param[out] txn  Destination transaction structure.
 */
static void deserialize(const uint8_t* buf, uint64_t* key, TxnData* txn) {
    const uint8_t* p = buf;

    txn->dateTime = *key;

    memcpy(&txn->status, p, sizeof(txn->status));
    p += sizeof(txn->status);

    memcpy(&txn->core.txnType, p, sizeof(txn->core.txnType));
    p += sizeof(txn->core.txnType);

    memcpy(&txn->core.mti, p, sizeof(txn->core.mti));
    p += sizeof(txn->core.mti);

    memcpy(&txn->core.processCode, p, sizeof(txn->core.processCode));
    p += sizeof(txn->core.processCode);

    memcpy(txn->core.pan, p, sizeof(txn->core.pan));
    p += sizeof(txn->core.pan);

    memcpy(&txn->core.amount, p, sizeof(txn->core.amount));
    p += sizeof(txn->core.amount);

    memcpy(&txn->core.rrn, p, sizeof(txn->core.rrn));
    p += sizeof(txn->core.rrn);

    memcpy(&txn->core.trace, p, sizeof(txn->core.trace));
    p += sizeof(txn->core.trace);

    memcpy(&txn->core.stan, p, sizeof(txn->core.stan));
    p += sizeof(txn->core.stan);

    memcpy(&txn->core.respCode, p, sizeof(txn->core.respCode));
    p += sizeof(txn->core.respCode);

    memcpy(&txn->extention, p, sizeof(txn->extention));
}

/**
 * @brief Deserialize and log a transaction record.
 *
 * Convenience helper used by iterateThrough().
 *
 * @param[in] rec Serialized transaction record.
 * @param[in] key Database key associated with the record.
 *
 * @return Error descriptor indicating the logging status.
 */
static ErrorDsc_t logRecord(uint8_t* rec, uint64_t* key) {
    TxnData txn;
    deserialize(rec, key, &txn);
    logTxnCore(&txn);
}

/**
 * @brief Insert a transaction into embedDB.
 *
 * The transaction is serialized and inserted using txn->dateTime as
 * its primary key.
 *
 * @param[in] txn Transaction to insert.
 *
 * @return Result_t containing the insertion status.
 */
static Result_t txnInsert(TxnData* txn) {
    TRACE_POINT;
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(txn, res.err = ERR_DSC_INVALID_ARG, res);
    RETURN_VALUE_IF_NULL(state, res.err = ERR_DSC_INVALID_ARG, res);
    TRACE_POINT;
    LOG_TRACE("Txn Record insert: time stamp = %llu", txn->dateTime);
    TRACE_POINT;
    uint32_t date, time;
    unpackDateTime(&txn->dateTime, &date, &time);
    uint8_t rec[TXN_RECORD_SIZE];
    TRACE_POINT;
    serialize(rec, txn);
    TRACE_POINT;
    logTxnCore(txn);
    if (embedDBPut(state, &txn->dateTime, rec) != 0) {
        LOG_ERROR("Transaction record: error in inserting record.");
        res.err       = ERR_DSC_DATABASE;
        res.detail.db = DB_ERR_INSERT_FAILURE;
        return res;
    }
    return res;
}

/**
 * @brief Iterate through all records in the transaction database.
 *
 * Records are read sequentially using an embedDB iterator and logged using
 * logRecord().
 *
 * @return Result_t containing the iteration status.
 */
static Result_t iterateThrough() {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(state, res.err = ERR_DSC_INVALID_ARG, res);
    embedDBIterator it;
    embedDBInitIterator(state, &it);
    uint64_t timeStamp;
    uint8_t  rec[TXN_RECORD_SIZE];
    while (embedDBNext(state, &it, &timeStamp, rec)) {
        logRecord(rec, &timeStamp);
    }
    return res;
}

/**
 * @brief Reset the transaction database.
 *
 * Clears the database files and resets embedDB to its initial state.
 *
 * @return Result_t containing the reset status.
 */
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

/**
 * @brief Initialize an empty query operator.
 *
 * Creates the query iterator and initial table-scan operator.
 *
 * @param[out] qo Query operator to initialize.
 *
 * @return Result_t containing the initialization status.
 */
static Result_t queryInit(QueryOperator* qo) {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    qo->limit.mode  = QUERY_LIMIT_NO;
    qo->limit.count = 0;
    qo->it          = (embedDBIterator*)MEM_ALLOC(sizeof(embedDBIterator));
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
    }
    return res;
}

/**
 * @brief Release resources associated with a query operator.
 *
 * Closes and frees the embedDB operator chain and iterator.
 *
 * @param[in,out] qo Query operator to close.
 *
 * @return Result_t containing the cleanup status.
 */
static Result_t queryClose(QueryOperator* qo) {
    Result_t res = {.err = ERR_DSC_OK};

    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);

    if (qo->op) {
        qo->op->close(qo->op);
        embedDBFreeOperatorRecursive(&qo->op);
    }

    if (qo->it) {
        embedDBCloseIterator(qo->it);
        EMDB_MEM_FREE(qo->it);
        qo->it = NULL;
    }

    return res;
}

/**
 * @brief Add a selection condition to a query.
 *
 * Wraps the current query operator with an embedDB selection operator.
 *
 * @param[in,out] qo Query operator.
 * @param[in] column Column on which the condition is applied.
 * @param[in] comparison Comparison operation.
 * @param[in] value Value used by the comparison.
 *
 * @return Result_t containing the operation status.
 */
static Result_t queryWhere(QueryOperator* qo, int column, int comparison,
                           void* value) {
    uint32_t* val = (uint32_t*)value;
    LOG_TRACE("Query where: column = %d, comparison = %d, value = %lu", column,
              comparison, *val);
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    RETURN_VALUE_IF_NULL(value, res.err = ERR_DSC_INVALID_ARG, res);
    embedDBOperator* newOp =
        createSelectionOperator(qo->op, column, comparison, value);
    RETURN_VALUE_IF_NULL(newOp, res.err = ERR_DSC_MEMORY, res);
    qo->op = newOp;
    return res;
}

// static Result_t orderBy(QueryOperator* qo, int column) {
//     Result_t res = {.err = ERR_DSC_OK};
//     RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
//     embedDBOperator* orderByOp = createOrderByOperator(
//         state, qo->op, column, -1, getColumnComparator(column));
//     LOG_TRACE("orderByOp = %p", (void*)orderByOp);
//     RETURN_VALUE_IF_NULL(orderByOp, res.err = ERR_DSC_MEMORY, res);
//     qo->op = orderByOp;
//     return res;
// }

/**
 * @brief Configure the result limit for a query.
 *
 * @param[in,out] qo Query operator.
 * @param[in] limit Limit configuration.
 *
 * @return Result_t containing the operation status.
 */
static Result_t limit(QueryOperator* qo, QueryLimit limit) {
    Result_t res = {.err = ERR_DSC_OK};
    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    qo->limit = limit;
    return res;
}

/**
 * @brief Insert a key into a bounded sorted key list.
 *
 * The function maintains at most limitCount keys.
 *
 * For QUERY_LIMIT_EARLIEST, keys are kept in ascending order and the
 * smallest keys are retained.
 *
 * For QUERY_LIMIT_LATEST, keys are kept in descending order and the
 * largest keys are retained.
 *
 * @param[in,out] keys       Sorted key array.
 * @param[in,out] count      Current number of stored keys.
 * @param[in]     limitCount Maximum number of keys to retain.
 * @param[in]     key        Key to insert.
 * @param[in]     mode       Ordering/selection mode.
 */
static void insertKey(uint64_t* keys, uint32_t* count, uint32_t limitCount,
                      uint64_t key, QueryLimitMode mode) {
    if (keys == NULL || count == NULL || limitCount == 0 ||
        mode == QUERY_LIMIT_NO) {
        return;
    }

    uint32_t n = *count;

    if (mode == QUERY_LIMIT_EARLIEST) {
        /* Ascending: smallest -> largest */

        uint32_t pos = 0;
        while (pos < n && keys[pos] < key) {
            pos++;
        }

        /* key is larger than everything we keep. */
        if (n == limitCount && pos == n) {
            return;
        }

        if (n < limitCount) {
            n++;
        }

        for (uint32_t i = n - 1; i > pos; --i) {
            keys[i] = keys[i - 1];
        }

        keys[pos] = key;
        *count    = n;
    } else if (mode == QUERY_LIMIT_LATEST) {
        /* Descending: largest -> smallest */

        uint32_t pos = 0;
        while (pos < n && keys[pos] > key) {
            pos++;
        }

        /* key is smaller than everything we keep. */
        if (n == limitCount && pos == n) {
            return;
        }

        if (n < limitCount) {
            n++;
        }

        for (uint32_t i = n - 1; i > pos; --i) {
            keys[i] = keys[i - 1];
        }

        keys[pos] = key;
        *count    = n;
    }
};

/**
 * @brief Deserialize and process a transaction from a query result buffer.
 *
 * The query operator buffer contains the database key followed by the
 * serialized transaction payload.
 *
 * @param[in] buf      Query result buffer.
 * @param[in] handler  Transaction callback.
 * @param[in] userData User-defined callback context.
 *
 * @return true if query processing should continue, false otherwise.
 */
static uint8_t ptRec[TXN_RECORD_SIZE];
static bool    processTxnRecord(const uint8_t* buf, TxnHandler handler,
                                void* userData) {
    uint64_t key;
    TxnData  data;

    memcpy(&key, buf, state->keySize);
    memcpy(ptRec, buf + state->keySize, TXN_RECORD_SIZE);

    deserialize(ptRec, &key, &data);

    logTxnCore(&data);

    return handler(&data, userData);
}

/**
 * @brief Execute an unlimited transaction query.
 *
 * Each matching record is deserialized and passed directly to the handler.
 *
 * @param[in,out] qo       Query operator.
 * @param[in]     handler  Transaction callback.
 * @param[in]     userData User-defined callback context.
 *
 * @return true if at least one transaction was found.
 */
static bool selectAllTransactions(QueryOperator* qo, TxnHandler handler,
                                  void* userData) {
    bool found = false;
    TRACE_POINT;
    TRACE_POINT;
    while (exec(qo->op)) {
        found = true;
        TRACE_POINT;
        TRACE_POINT;
        if (!processTxnRecord((const uint8_t*)qo->op->recordBuffer, handler,
                              userData)) {
            break;
        }
    }

    return found;
}

/**
 * @brief Execute a limited query and collect matching transaction keys.
 *
 * All matching records are scanned, but only the keys required by the
 * configured limit are retained in sortedkeys.
 *
 * @param[in,out] qo Query operator containing the limit configuration.
 *
 * @return Number of keys retained in sortedkeys.
 */
static uint32_t collectLimitedKeys(QueryOperator* qo) {
    uint32_t count = 0;

    memset(sortedkeys, 0, sizeof(sortedkeys));

    while (exec(qo->op)) {
        uint64_t key;

        memcpy(&key, qo->op->recordBuffer, state->keySize);

        insertKey(sortedkeys, &count, qo->limit.count, key, qo->limit.mode);
    }

    return count;
}

/**
 * @brief Retrieve and process transactions identified by sorted keys.
 *
 * Each key is retrieved from embedDB, deserialized, and passed to the
 * transaction handler in the order maintained by insertKey().
 *
 * @param[in] count Number of keys to process.
 * @param[in] handler Transaction callback.
 * @param[in] userData User-defined callback context.
 */
static void processLimitedTransactions(uint32_t count, TxnHandler handler,
                                       void* userData) {
    uint8_t rec[TXN_RECORD_SIZE];
    TxnData data;

    for (uint32_t i = 0; i < count; ++i) {
        uint64_t key = sortedkeys[i];

        if (embedDBGet(state, &key, rec) != 0) {
            LOG_ERROR("Failed to get transaction with key = %llu", key);
            continue;
        }

        deserialize(rec, &key, &data);

        if (!handler(&data, userData)) {
            break;
        }
    }
}

/**
 * @brief Execute a configured transaction query.
 *
 * Handles both unlimited queries and limited earliest/latest queries.
 *
 * @param[in,out] qo       Configured query operator.
 * @param[in]     handler  Callback invoked for each selected transaction.
 * @param[in]     userData User-defined callback context.
 *
 * @return ERR_DSC_OK when matching records are found, ERR_DSC_NOT_FOUND
 *         when no records match, or an appropriate error code on failure.
 */
static Result_t txnSelect(QueryOperator* qo, TxnHandler handler,
                          void* userData) {
    Result_t res;
    TRACE_POINT;
    TRACE_POINT;
    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    RETURN_VALUE_IF_NULL(handler, res.err = ERR_DSC_INVALID_ARG, res);

    (qo->op)->init(qo->op);
    LOG_TRACE("Txn select: limit mode = %d, limit count = %lu", qo->limit.mode,
              qo->limit.count);
    TRACE_POINT;
    TRACE_POINT;

    if (qo->limit.mode == QUERY_LIMIT_NO) {
        bool found = selectAllTransactions(qo, handler, userData);

        res.err = found ? ERR_DSC_OK : ERR_DSC_NOT_FOUND;
        return res;
    }
    TRACE_POINT;
    TRACE_POINT;
    uint32_t count = collectLimitedKeys(qo);

    if (count == 0) {
        res.err = ERR_DSC_NOT_FOUND;
        return res;
    }
    TRACE_POINT;
    TRACE_POINT;
    processLimitedTransactions(count, handler, userData);

    res.err = ERR_DSC_OK;
    return res;
}

static TxnType activeTxns[] = {
    TXN_PURCHASE,
    TXN_BILL,
    TXN_TOPUP,
    TXN_VOUCHER,
};

static bool handleAggData(const TxnData* txn, void* userData) {
    AggDataSummary* summary = (AggDataSummary*)userData;
    LOG_TRACE("aggregate: summary->txnCount = %lu", summary->txnCount);
    for (size_t i = 0; i < summary->txnCount; i++) {
        LOG_TRACE("aggregate: txn type = %d, txn count = %llu, txn sum = %llu",
                  summary->data[i].txn, summary->data[i].count,
                  summary->data[i].sum);
        if (summary->data[i].txn == txn->core.txnType) {
            summary->data[i].count++;
            summary->data[i].sum += txn->core.amount;
        }
    }
    return true;
}

static Result_t aggregate(QueryOperator* qo, TxnType txntype,
                          TxnAggregateHandler handler, void* userData) {
    Result_t res = {.err = ERR_DSC_OK};

    RETURN_VALUE_IF_NULL(qo, res.err = ERR_DSC_INVALID_ARG, res);
    RETURN_VALUE_IF_NULL(qo->op, res.err = ERR_DSC_INVALID_ARG, res);
    size_t   size    = ARRAY_SIZE(activeTxns);
    AggData* aggData = MEM_ALLOC(sizeof(AggData) * size);
    RETURN_VALUE_IF_NULL(aggData, res.err = ERR_DSC_NO_SPACE, res);
    memset(aggData, 0, sizeof(AggData) * size);
    for (size_t i = 0; i < size; i++) {
        aggData[i].txn = activeTxns[i];
    }
    uint32_t txntype32 = txntype;
    if (txntype != TXN_ALL) {
        res = queryWhere(qo, TXN_REC_COL_TYPE, SELECT_EQ, &txntype32);
        RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, res);
    }

    AggDataSummary summary = {.data = aggData, .txnCount = size};
    (qo->op)->init(qo->op);
    bool found = selectAllTransactions(qo, handleAggData, &summary);
    RETURN_VALUE_IF(found, false, res.err = ERR_DSC_NOT_FOUND;, res);
    if (handler) {
        handler(txntype, &summary, userData);
    }
    MEM_FREE(aggData);
    return res;
}

/**
 * @brief Initialize the transaction database and its schema.
 *
 * Allocates the embedDB state, configures the database, and creates the
 * transaction record schema used by query operators.
 *
 * @param[in,out] self Transaction record service instance.
 *
 * @return Result_t containing the initialization status.
 */
static Result_t init(TxnRecord* self) {
    Result_t res = {.err = ERR_DSC_OK};
    state        = (embedDBState*)EMDB_MEM_ALLOC(sizeof(embedDBState));
    RETURN_VALUE_IF_NULL(state, res.err = ERR_DSC_MEMORY, res);

    uint32_t parameters = EMBEDDB_RECORD_LEVEL_CONSISTENCY |
                          (EMBEDDB_USE_BMAP | EMBEDDB_USE_INDEX);
    LOG_ERROR("size of txnData = %u.", TXN_RECORD_SIZE);

    if (embedDBSetup(state, TRANS_RECORD_PATH, TRANS_IDX_PATH, sizeof(uint64_t),
                     TXN_RECORD_SIZE, PAGE_SIZE_512, PAGE_NUMBER,
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
        state->keySize,                          // key
        sizeof(((TxnData*)0)->status),           // status
        sizeof(((TxnData*)0)->core.txnType),     // type
        sizeof(((TxnData*)0)->core.mti),         // mti
        sizeof(((TxnData*)0)->core.processCode), // processCode
        sizeof(((TxnData*)0)->core.pan),         // maskedPan
        sizeof(((TxnData*)0)->core.amount),      // amount
        sizeof(((TxnData*)0)->core.rrn),         // rrn
        sizeof(((TxnData*)0)->core.trace),       // trace
        sizeof(((TxnData*)0)->core.stan),        // stan
        sizeof(((TxnData*)0)->core.respCode),    // responseCode
        sizeof(((TxnData*)0)->extention)         // extention
    };

    int8_t colSignedness[] = {
        embedDB_COLUMN_UNSIGNED, // key
        embedDB_COLUMN_UNSIGNED, // status
        embedDB_COLUMN_UNSIGNED, // type
        embedDB_COLUMN_UNSIGNED, // mti
        embedDB_COLUMN_UNSIGNED, // processCode
        embedDB_COLUMN_UNSIGNED, // maskedPan
        embedDB_COLUMN_UNSIGNED, // amount
        embedDB_COLUMN_UNSIGNED, // rrn
        embedDB_COLUMN_UNSIGNED, // trace
        embedDB_COLUMN_UNSIGNED, // stan
        embedDB_COLUMN_UNSIGNED, // responseCode
        embedDB_COLUMN_UNSIGNED  // extention
    };

    ColumnType colTypes[] = {
        embedDB_COLUMN_UINT64, // key
        embedDB_COLUMN_UINT32, // status
        embedDB_COLUMN_UINT32, // type
        embedDB_COLUMN_UINT32, // mti
        embedDB_COLUMN_UINT32, // processCode
        embedDB_COLUMN_UINT32, // pan
        embedDB_COLUMN_UINT64, // amount
        embedDB_COLUMN_UINT64, // rrn
        embedDB_COLUMN_UINT32, // trace
        embedDB_COLUMN_UINT32, // stan
        embedDB_COLUMN_UINT32, // responseCode (uint16_t)
        embedDB_COLUMN_UINT32  // extention
    };

    uint8_t columnNum = (sizeof(colSizes) / sizeof(colSizes[0]));
    LOG_TRACE("Schema: culomn size = %d,", columnNum);

    size_t schemaRecordSize =
        sizeof(((TxnData*)0)->status) + sizeof(((TxnData*)0)->core.txnType) +
        sizeof(((TxnData*)0)->core.mti) +
        sizeof(((TxnData*)0)->core.processCode) +
        sizeof(((TxnData*)0)->core.pan) + sizeof(((TxnData*)0)->core.amount) +
        sizeof(((TxnData*)0)->core.rrn) + sizeof(((TxnData*)0)->core.trace) +
        sizeof(((TxnData*)0)->core.stan) +
        sizeof(((TxnData*)0)->core.respCode) + sizeof(((TxnData*)0)->extention);

    LOG_TRACE("TxnDbRecord size = %zu, schema data size = %zu", TXN_RECORD_SIZE,
              schemaRecordSize);

    schema = embedDBCreateSchema(columnNum, colSizes, colSignedness, colTypes);
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

/**
 * @brief Construct the TxnRecord service.
 *
 * Assigns the implementation functions to the TxnRecord interface.
 *
 * @param[in,out] self Transaction record service instance.
 */
OOP_CTOR(TxnRecord) {
    self->init      = init;
    self->insert    = txnInsert;
    self->select    = txnSelect;
    self->reset     = txnReset;
    self->iterate   = iterateThrough;
    self->aggregate = aggregate;
}

TxnRecord* txnrecord() {
    CALL_ONCE(OOP_CALL_CTOR(TxnRecord, &__txnrecord););
    return &__txnrecord;
}

OOP_CTOR(TxnQuery) {
    self->init  = queryInit;
    self->where = queryWhere;
    self->limit = limit;
    self->close = queryClose;
}

TxnQuery* txnquery() {
    CALL_ONCE(OOP_CALL_CTOR(TxnQuery, &__txnquery););
    return &__txnquery;
}