#ifndef EMV_KAHROBA_H
#define EMV_KAHROBA_H

#include "emvMgr.h"
#include "picc/picc.h"
#include "txn.h"

typedef struct {
    /* READ RECORD: CDOL1 definition */
    uint8_t cdol1[128];
    size_t  cdol1Len;

    /* READ RECORD: cardholder data */
    uint8_t pan[10];
    size_t  panLen;

    uint8_t track2[19];
    size_t  track2Len;

    /* Optional proprietary value returned in record 2 */
    uint8_t df70;
    bool    hasDf70;

} KahrobaCardData;

typedef struct {
    const char* amountDigits;

    uint8_t country[2];
    uint8_t currency[2];

    uint8_t tvr[5];
    uint8_t date[3];
    uint8_t time[3];
    uint8_t unpredictableNumber[4];
    uint8_t cvmResult[3];

    uint8_t transactionType;
    uint8_t terminalType;

    const uint8_t* merchantName;
    size_t         merchantNameLen;
} KahrobaAcData;

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
