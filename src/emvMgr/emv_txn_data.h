#ifndef EMV_TXN_DATA_H
#define EMV_TXN_DATA_H

#include "emvMgr.h"

/*
 * Transaction-owned EMV protocol data; embed this struct in TxnData (or
 * equivalent transient transaction context). Never persist it as-is.
 * Set capacities to the values authorized by your host protocol contract.
 */
#ifndef EMV_TXN_FIELD55_CAPACITY
#define EMV_TXN_FIELD55_CAPACITY 512u
#endif
#ifndef EMV_TXN_PAN_BCD_CAPACITY
#define EMV_TXN_PAN_BCD_CAPACITY 10u
#endif
#ifndef EMV_TXN_TRACK2_BCD_CAPACITY
#define EMV_TXN_TRACK2_BCD_CAPACITY 19u
#endif

typedef struct {
    uint8_t field55[EMV_TXN_FIELD55_CAPACITY]; /**< Owned binary ISO 8583
                                                  field 55. */
    size_t field55_len; /**< Actual field 55 byte length. */

    /* Sensitive transient card data (only expose to the ISO builder). */
    uint8_t pan_bcd[EMV_TXN_PAN_BCD_CAPACITY]; /**< Tag 5A, if provided. */
    size_t  pan_bcd_len;
    uint8_t
           track2_bcd[EMV_TXN_TRACK2_BCD_CAPACITY]; /**< Tag 57, if provided. */
    size_t track2_bcd_len;

    uint8_t aid[16]; /**< Selected application identifier, tag 84. */
    size_t  aid_len;
    uint8_t
        cryptogram[8]; /**< Tag 9F26. Online authorization is still pending. */
    uint8_t atc[2];    /**< Tag 9F36. */
    uint8_t cid;       /**< Tag 9F27. */
} EmvTxnData;

/**
 * Atomically replace destination with an owned, bounded copy of the result.
 * Only EMV_OUTCOME_ONLINE_REQUIRED may be imported.
 * On failure, destination is cleared to avoid reusing an older transaction.
 */
EmvError emvTxnDataImport(EmvTxnData* destination, const EmvResult* result,
                          const uint8_t* field55, size_t field55_len);

/** Securely erase all transaction-owned EMV/card data after use. */
void emvTxnDataClear(EmvTxnData* data);

#endif
