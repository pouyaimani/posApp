#ifndef TXNRECS_H_
#define TXNRECS_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "embedDB/embedDB.h"
#include "embedDB/query-interface/advancedQueries.h"
#include "txn.h"
#include "error.h"

typedef enum {
    TXN_REC_COL_KEY = 0,   // key
    TXN_REC_COL_STATUS,    // status
    TXN_REC_COL_TYPE,      // type
    TXN_REC_COL_MTI,       // mti
    TXN_REC_COL_PRCODE,    // processCode
    TXN_REC_COL_PAN,       // pan
    TXN_REC_COL_AMNT,      // amount
    TXN_REC_COL_REFNUM,    // rrn
    TXN_REC_COL_TRACE,     // trace
    TXN_REC_COL_RRN,       // stan
    TXN_REC_COL_RESP_CODE, // responseCode
    TXN_REC_COL_EXTENTION, // extention
} TxnRecColumns_t;

typedef uint64_t TxnIndex_t;

#define QUERY_NO_LIMIT -1

typedef enum {
    QUERY_LIMIT_NO,
    QUERY_LIMIT_LATEST,
    QUERY_LIMIT_EARLIEST
} QueryLimitMode;

typedef struct {
    QueryLimitMode mode;
    uint32_t       count;
} QueryLimit;

typedef struct {
    embedDBOperator* op;
    embedDBIterator* it;
    QueryLimit       limit;
} QueryOperator;

typedef bool (*TxnHandler)(const TxnData* rec, void* userData);

OOP_CLASS(TxnQuery) {
    OOP_METHOD(Result_t, init, QueryOperator*);
    OOP_METHOD(Result_t, where, QueryOperator * operator, int column, int,
               void* value);
    OOP_METHOD(Result_t, limit, QueryOperator * op, QueryLimit limit);
};

TxnQuery* txnquery(void);

OOP_CLASS(TxnRecord) {
    OOP_METHOD(Result_t, init);
    OOP_METHOD(Result_t, insert, TxnData*);
    OOP_METHOD(Result_t, iterate);
    OOP_METHOD(Result_t, select, QueryOperator*, TxnHandler handler,
               void* userData);
    OOP_METHOD(Result_t, reset);
};

TxnRecord* txnrecord(void);

#endif