#ifndef EMV_KAHROBA_H
#define EMV_KAHROBA_H

#include "emvMgr.h"
#include "picc/picc.h"
#include "txn.h"

typedef struct {
    const char* amountDigits;

    uint8_t ttpi[5];
    uint8_t currency[2];
    uint8_t country[2];

    uint8_t terminalType;
    uint8_t transactionType;
} KahrobaGpoData;

EmvMgrError startKahrobaEmv(Picc* reader, TxnData* txn);

#endif
