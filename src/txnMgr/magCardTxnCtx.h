#ifndef MAG_CARD_TXN_H
#define MAG_CARD_TXN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "txn.h"

typedef struct {
    /* PAN returned by the magnetic reader. */
    char pan[LEN_MAX_PAN + 1u];

    /* Raw Track 2 returned by the reader (not ISO padded). */
    char   track2[LEN_MAX_TRACK2 + 1u];
    size_t track2Len;

    /* True only after all required card data was captured. */
    bool ready;

} MagCardTxnContext;

/**
 * Capture magnetic-card data into a transaction-owned context.
 * Must be called during card input, not during ISO building.
 */
bool magCardTxnCapture(MagCardTxnContext* context, TxnData* txn);

/** Clear sensitive card information when no longer needed. */
void magCardTxnClear(MagCardTxnContext* context);

#endif