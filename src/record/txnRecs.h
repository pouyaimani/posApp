#ifndef TXNRECS_H_
#define TXNRECS_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "embedDB/embedDB.h"
#include "embedDB/query-interface/advancedQueries.h"
#include "txn.h"
#include "error.h"

typedef struct {
    uint32_t  groupValue;
    uint32_t  count;
    uint64_t* sums;
} TxnAggregateResult;

/**
 * @brief Column indices used by the transaction database query interface.
 *
 * The order of these columns must match the order used when creating the
 * embedDB schema in txnRecord initialization.
 */
typedef enum {
    TXN_REC_COL_KEY = 0,   /**< Transaction timestamp/key. */
    TXN_REC_COL_STATUS,    /**< Transaction status. */
    TXN_REC_COL_TYPE,      /**< Transaction type. */
    TXN_REC_COL_MTI,       /**< ISO8583 Message Type Indicator (MTI). */
    TXN_REC_COL_PRCODE,    /**< ISO8583 processing code. */
    TXN_REC_COL_PAN,       /**< Masked transaction PAN. */
    TXN_REC_COL_AMNT,      /**< Transaction amount. */
    TXN_REC_COL_RRN,       /**< Retrieval Reference Number (RRN). */
    TXN_REC_COL_TRACE,     /**< Transaction trace number. */
    TXN_REC_COL_STAN,      /**< System Trace Audit Number (STAN). */
    TXN_REC_COL_RESP_CODE, /**< Transaction response code. */
    TXN_REC_COL_EXTENTION, /**< Transaction extension data. */
} TxnRecColumns_t;

/**
 * @brief Type used as the primary key of a transaction record.
 *
 * Transaction records are indexed by their timestamp/key. The key is stored
 * as a 64-bit unsigned integer and is also used to determine transaction
 * ordering for earliest/latest queries.
 */
typedef uint64_t TxnIndex_t;

/**
 * @brief Legacy value representing an unlimited query.
 *
 * @deprecated Use QUERY_LIMIT_NO instead.
 */
#define QUERY_NO_LIMIT -1

/**
 * @brief Specifies how a query result should be limited.
 */
typedef enum {
    QUERY_LIMIT_NO,      /**< Return all matching records. */
    QUERY_LIMIT_LATEST,  /**< Return the latest matching records. */
    QUERY_LIMIT_EARLIEST /**< Return the earliest matching records. */
} QueryLimitMode;

/**
 * @brief Describes the record limit and ordering of a transaction query.
 */
typedef struct {
    QueryLimitMode mode;  /**< Limit mode used by the query. */
    uint32_t       count; /**< Maximum number of records to return. */
} QueryLimit;

/**
 * @brief Runtime state of a transaction query.
 *
 * A QueryOperator owns the embedDB iterator and operator chain used to
 * execute a transaction query.
 */
typedef struct {
    embedDBOperator* op;    /**< Current embedDB query operator. */
    embedDBIterator* it;    /**< Iterator associated with the query. */
    QueryLimit       limit; /**< Result limit and ordering configuration. */
} QueryOperator;

/**
 * @brief Callback invoked for each transaction returned by a query.
 *
 * The callback is called once for every transaction selected by the query.
 * Returning false stops query processing early.
 *
 * @param[in] rec      Transaction returned by the query.
 * @param[in] userData User-defined context passed to the callback.
 *
 * @return true to continue processing records, false to stop processing.
 */
typedef bool (*TxnHandler)(const TxnData* rec, void* userData);

typedef bool (*TxnAggregateHandler)(TxnType txnType, uint32_t count,
                                    const uint64_t* sums, uint32_t sumCount,
                                    void* userData);

/**
 * @brief Transaction query builder.
 *
 * TxnQuery provides a small query-building interface around embedDB.
 *
 * Typical usage:
 *
 * @code
 * QueryOperator query;
 *
 * txnquery()->init(&query);
 * txnquery()->where(&query, TXN_REC_COL_STATUS, SELECT_EQ, &status);
 * txnquery()->limit(&query,
 *                   (QueryLimit){QUERY_LIMIT_LATEST, 10});
 *
 * txnrecord()->select(&query, handler, userData);
 *
 * txnquery()->close(&query);
 * @endcode
 */
OOP_CLASS(TxnQuery) {

    /**
     * @brief Initialize a transaction query.
     *
     * Creates the embedDB iterator and initial table-scan operator.
     *
     * @param[out] operator Query operator to initialize.
     *
     * @return Result_t containing the operation status.
     */
    OOP_METHOD(Result_t, init, QueryOperator*);

    /**
     * @brief Add a filtering condition to a transaction query.
     *
     * The new selection operator is appended to the existing operator chain.
     *
     * @param[in,out] operator Query operator.
     * @param[in] column Column to compare. Use TxnRecColumns_t values.
     * @param[in] comparison Comparison operation, such as SELECT_EQ,
     *                        SELECT_GT, etc.
     * @param[in] value Value used for the comparison.
     *
     * @return Result_t containing the operation status.
     */
    OOP_METHOD(Result_t, where, QueryOperator * operator, int column, int,
               void* value);

    /**
     * @brief Configure the maximum number of records returned by a query.
     *
     * @param[in,out] op    Query operator.
     * @param[in] limit     Limit mode and maximum record count.
     *
     * @return Result_t containing the operation status.
     */
    OOP_METHOD(Result_t, limit, QueryOperator * op, QueryLimit limit);

    /**
     * @brief Close and release all resources owned by a query.
     *
     * This releases the embedDB operator chain and query iterator.
     *
     * @param[in,out] operator Query operator to close.
     *
     * @return Result_t containing the operation status.
     */
    OOP_METHOD(Result_t, close, QueryOperator*);
};

/**
 * @brief Returns the global transaction query service instance.
 *
 * The instance is constructed once and reused by subsequent calls.
 *
 * @return Pointer to the transaction query service.
 */
TxnQuery* txnquery(void);

/**
 * @brief Transaction record database interface.
 *
 * Provides initialization, insertion, selection, iteration, and reset
 * operations for the transaction database.
 */
OOP_CLASS(TxnRecord) {

    /**
     * @brief Initialize the transaction database.
     *
     * Creates and configures the embedDB instance and transaction schema.
     *
     * @return Result_t containing the initialization status.
     */
    OOP_METHOD(Result_t, init);

    /**
     * @brief Insert a transaction into the database.
     *
     * The transaction is serialized and stored using its dateTime value
     * as the database key.
     *
     * @param[in] txn Transaction to insert.
     *
     * @return Result_t containing the insertion status.
     */
    OOP_METHOD(Result_t, insert, TxnData*);

    /**
     * @brief Iterate through all transaction records.
     *
     * Each transaction encountered during iteration is logged.
     *
     * @return Result_t containing the iteration status.
     */
    OOP_METHOD(Result_t, iterate);

    /**
     * @brief Select transactions using a configured query.
     *
     * Executes the supplied query and invokes the supplied handler for each
     * matching transaction.
     *
     * If the query has QUERY_LIMIT_NO configured, matching records are
     * processed directly. For QUERY_LIMIT_LATEST or QUERY_LIMIT_EARLIEST,
     * matching keys are first collected and sorted before the corresponding
     * records are retrieved.
     *
     * Returning false from the handler stops processing.
     *
     * @param[in,out] operator Query operator containing the query definition.
     * @param[in] handler Callback invoked for each selected transaction.
     * @param[in] userData User-defined context passed to the callback.
     *
     * @return ERR_DSC_OK if at least one record was found,
     *         ERR_DSC_NOT_FOUND if no records matched,
     *         or an appropriate error code on invalid input.
     */
    OOP_METHOD(Result_t, select, QueryOperator*, TxnHandler handler,
               void* userData);

    OOP_METHOD(Result_t, aggregate, QueryOperator * qo, const int* columns,
               uint32_t columnCount, uint32_t* count, uint64_t* sums);

    /**
     * @brief Reset the transaction database.
     *
     * Removes the existing transaction database contents and recreates the
     * database using the configured database files.
     *
     * @return Result_t containing the reset status.
     */
    OOP_METHOD(Result_t, reset);
};

/**
 * @brief Returns the global transaction record service instance.
 *
 * The instance is constructed once and reused by subsequent calls.
 *
 * @return Pointer to the transaction record service.
 */
TxnRecord* txnrecord(void);

#endif /* TXNRECS_H_ */