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
    TXN_REC_FIELD_TIMESTAMP = 0,
    TXN_REC_FIELD_CORE_ID,
    TXN_REC_FIELD_CORE_PROCESS_CODE,
    TXN_REC_FIELD_CORE_PAN,
    TXN_REC_FIELD_CORE_AMOUNT,
    TXN_REC_FIELD_CORE_REF_NUM,
    TXN_REC_FIELD_CORE_TRACE,
    TXN_REC_FIELD_CORE_RRN,
    TXN_REC_FIELD_CORE_RESP_CODE,
} TxnRecField_t;

typedef uint64_t TxnIndex_t;

typedef struct {
    embedDBOperator* op;
    embedDBIterator* it;
} QueryOperator;

typedef bool (*TxnHandler)(const TxnData* rec, void* userData);

OOP_CLASS(TxnQuery) {
    OOP_METHOD(int8_t, init, QueryOperator*);
    OOP_METHOD(void, where, QueryOperator* operator, int column, int,
               void* value);
};

TxnQuery* txnquery(void);

OOP_CLASS(TxnRecord) {
    OOP_METHOD(, init);
    OOP_METHOD(Result_t, insert, TxnData*);
    OOP_METHOD(Result_t, iterate);
    OOP_METHOD(Result_t, select, QueryOperator*, TxnHandler handler,
               void* userData);
    OOP_METHOD(Result_t, reset);
};

TxnRecord* txnrecord(void);

#endif