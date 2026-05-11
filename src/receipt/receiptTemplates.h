#ifndef RECEIPT_TEMPLATE_H_
#define RECEIPT_TEMPLATE_H_

#include <stdint.h>
#include "txn.h"
#include "receipt.h"

typedef int8_t (*ReceiptBuilder)(
    Receipt *,
    const TxnData *);

typedef struct {
    TxnType_t type;
    ReceiptBuilder builder;
} ReceiptTemplate;

int8_t buildReceipt(
    Receipt *rec,
    TxnType_t type,
    const TxnData *txn);

#endif