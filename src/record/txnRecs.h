#ifndef TXNRECS_H_
#define TXNRECS_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "embedDB/embedDB.h"
#include "embedDB/query-interface/advancedQueries.h"
#include "txn.h"

enum TxnField {
	TXN_KEY = 0,
    TXN_ID,
    TXN_PROCESS_CODE,
    TXN_MASKED_PAN,
    TXN_PURCHASE_ID,
    TXN_AMOUNT,
    TXN_PRICE_WITH_DISCOUNT,
    TXN_REF_NUM,
    TXN_TRACE,
    TXN_DATETIME,
    TXN_RRN,
    TXN_BILL_ID,
    TXN_PAYMENT_ID,
    TXN_COMPANY_ID,
    TXN_COMPANY_NAME,
    TXN_PHONE_NUMBER,
    TXN_CHARGE_LEVEL,
    TXN_ACCOUNT_INDEX,
    TXN_ACCOUNT_CAPTION,
    TXN_RESPONSE_CODE,
    TXN_STATUS,
    TXN_FIELD_COUNT
};

typedef uint64_t TxnIndex_t;

typedef struct {
	embedDBOperator *op;
	embedDBIterator *it;
} QueryOperator;

typedef bool (*TxnHandler)(const TxnData* rec, void* userData);

OOP_CLASS(TxnQuery) {
	OOP_METHOD(int8_t, init, QueryOperator *);
	OOP_METHOD(void, where, QueryOperator *operator, int column, int, void *value);
};

TxnQuery *txnquery(void);

OOP_CLASS(TxnRecord) {
	OOP_METHOD(void, init);
    OOP_METHOD(void, insert, TxnData *);
	OOP_METHOD(void, iterate);
	OOP_METHOD(void, select, QueryOperator *, TxnHandler handler);
    OOP_METHOD(void, reset);
};

TxnRecord *txnrecord(void);

#endif