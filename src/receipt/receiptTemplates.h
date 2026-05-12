#ifndef RECEIPT_TEMPLATE_H_
#define RECEIPT_TEMPLATE_H_

#include <stdint.h>
#include "txn.h"
#include "receipt.h"

typedef int8_t (*ReceiptBuilder)(
    Receipt *,
    const TxnData *);

typedef enum {
    REC_SALE = 0,
	REC_BILL,
	REC_TOPUP,
	REC_BALANCE,
	REC_PAY,
	REC_SIM_CHARGE_CODE,
    REC_DAILY_REPORT_HEADER,
    REC_DAILY_REPORT_BODY,
    REC_AGGREGATION_REPORT_HEADER,
    REC_AGGREGATION_REPORT_BODY,
    REC_DETAIL_REPORT_HEADER,
    REC_DETAIL_REPORT_BODY
} ReceiptType_t;

typedef struct {
    TxnType_t type;
    ReceiptBuilder builder;
} ReceiptTemplate;

int8_t buildReceipt(
    Receipt *rec,
    TxnType_t type,
    const TxnData *txn);

#endif