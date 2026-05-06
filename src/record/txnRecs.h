#ifndef TXNRECS_H_
#define TXNRECS_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "embedDB/embedDB.h"
#include "embedDB/query-interface/advancedQueries.h"
#include "txn.h"

typedef struct {
	long long timeStamp;
} TxnIndex_t;

typedef enum {
	QUERY_CUL_DATE_TIME = 0,
	QUERY_CUL_STAN,
	QUERY_CUL_REF_NUM,
	QUERY_CUL_TXN_TYPE
} QueryColumn_t;

typedef struct {
	embedDBIterator *it;
	embedDBOperator *op;
} QueryOperation_t;

typedef bool (*TxnHandler)(const TxnData* rec, void* userData);

OOP_CLASS(TxnQuery) {
	OOP_METHOD(void, init, QueryOperation_t *);
	OOP_METHOD(void, where, QueryOperation_t *, QueryColumn_t, int, void *value);
};

TxnQuery *txnquery(void);

OOP_CLASS(TxnRecord) {
    OOP_METHOD(void, insert, TxnData *);
	OOP_METHOD(void, select, QueryOperation_t *, TxnHandler handler);
    OOP_METHOD(void, reset);
};

TxnRecord *txnrecord(void);

int doTest(void);

#endif