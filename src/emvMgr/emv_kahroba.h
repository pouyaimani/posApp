#ifndef EMV_KAHROBA_H
#define EMV_KAHROBA_H

#include "emvMgr.h"
#include "picc/picc.h"
#include "txn.h"
#include "kahroba_field55.h"

typedef struct {
    uint8_t cdol1[128];
    size_t  cdol1Len;

    uint8_t pan[10];
    size_t  panLen;

    uint8_t track2[19];
    size_t  track2Len;

    uint8_t df70;
    bool    hasDf70;

    /* 9F6E - Form Factor Indicator */
    uint8_t formFactor[4];
    bool    hasFormFactor;

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

typedef struct {
    /* Data extracted from READ RECORD. */
    KahrobaCardData card;

    /* Data returned by GENERATE AC. */
    EmvGenerateAcResult acResult;

    /* Values used in GENERATE AC and later Field 55. */
    uint8_t unpredictableNumber[4];
    uint8_t tvr[5];
    uint8_t cvmResult[3];

    uint8_t transactionDate[3];
    uint8_t transactionTime[3];

    uint8_t currency[2];
    uint8_t country[2];

    uint8_t transactionType;
    uint8_t terminalType;

    uint8_t aip[2]; /* 82 - Application Interchange Profile */
    bool    hasAip;

    uint64_t authorizedAmount; /* Same amount supplied to GPO and GEN AC */
} KahrobaEmvContext;

typedef struct {
    KahrobaEmvContext emv;

    /* Binary Field 55 data for online authorization. */
    uint8_t field55[KAHROBA_FIELD55_MAX_LEN];
    size_t  field55Len;

    bool emvReady;
} KahrobaTxnSession;

EmvMgrError startKahrobaEmv(Picc* reader, TxnData* txn,
                            KahrobaEmvContext* context);

/**
 * @brief Build binary Field 55 for Kahroba authorization.
 *
 * Requires a completed EMV session containing an ARQC.
 *
 * The output contains sensitive payment data.
 * It must not be logged or persisted.
 */
bool kahrobaBuildField55(const TxnData* txn, const KahrobaEmvContext* context,
                         uint8_t* output, size_t capacity,
                         size_t* outputLength);

#endif
