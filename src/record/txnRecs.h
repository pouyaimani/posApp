#ifndef TXNRECS_H_
#define TXNRECS_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
	TS_START = 0,
	TS_BUILD_REQUEST = 1,

	TS_REQUEST_SEND = 11,
	TS_REQUEST_RECIEVE = 12,

	TS_REQUEST_HAVE_RESPONSE = 3,

	TS_REQUEST_SUCCEED = 30,
	TS_REQUEST_SUCCEED_PRINT = 31,
	TS_REQUEST_SUCCEED_FINALIZED = 100,

	TS_REQUEST_FAILED = 35,
	TS_REQUEST_FAILED_PRINT = 36,
	TS_REQUEST_FAILED_FINALIZED = 101,

	TS_REQUEST_HAVE_NO_RESPONSE = 4,
	TS_REQUEST_REVERSE_PRINT_72H = 40,
	TS_REQUEST_REVERSE_FINALIZED = 110,
} TransactionStatus;

typedef enum {
	CL_NORMAL = 1,
	CL_SPECIFIC = 2,
} ChargeLevel;

typedef struct {
	int id;
	char ProcessCode[6+1];
	char MaskedPan[16+1];
	char PurchaseId[31];
	char Amount[12+1];
	char PriceWithDiscount[12+1];
	char STAN[6+1];
	char Trace[6+1];
	char DateTime[14+1];
	long long RawTime;
	char RRN[12+1];
	char BillId[24];
	char PaymentId[24];
	unsigned long companyId;
	char companyName[64]; // 116 kahroba
	char CellphonNumber[11+1]; // For Kahroba
	ChargeLevel chargeLevel;
	char AccountIndex[8];
	char AccountCaption[32+1];
	char ResponseCode[2+1];

	TransactionStatus  Status;
} TxnData_t;

OOP_CLASS(TxnRecord) {
    OOP_METHOD(void, insert, TxnData_t *);
    OOP_METHOD(void, get, TxnData_t *);
	OOP_METHOD(int, getLatest, TxnData_t *);
    OOP_METHOD(void, reset);
};

TxnRecord *txnrecord(void);

int doTest(void);

#endif