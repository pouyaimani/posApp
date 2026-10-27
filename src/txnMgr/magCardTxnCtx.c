#include "magCardTxnCtx.h"

#include <string.h>

#include "logger.h"
#include "magReader/magReader.h"

void magCardTxnClear(MagCardTxnContext* context) {
    if (!context)
        return;

    volatile uint8_t* p = (volatile uint8_t*)context;

    for (size_t i = 0u; i < sizeof(*context); ++i)
        p[i] = 0u;
}

bool magCardTxnCapture(MagCardTxnContext* context, TxnData* txn) {
    if (!context || !txn)
        return false;

    magCardTxnClear(context);

    /* Capture PAN. */
    magreader()->getPan(context->pan, sizeof(context->pan));

    size_t panLen = 0u;

    while (panLen < sizeof(context->pan) && context->pan[panLen] != '\0') {
        ++panLen;
    }

    if (panLen < 12u || panLen > LEN_MAX_PAN) {
        LOG_TRACE("MagCard: invalid PAN length");
        goto failed;
    }

    for (size_t i = 0u; i < panLen; ++i) {
        if (context->pan[i] < '0' || context->pan[i] > '9') {
            LOG_TRACE("MagCard: invalid PAN format");
            goto failed;
        }
    }

    /* Capture Track 2 once. */
    TrackData_t track = OOP_CALL(magreader(), getTrack2);

    if (!track.data || track.len <= 0 || (size_t)track.len > LEN_MAX_TRACK2) {

        LOG_TRACE("MagCard: invalid Track2 length");
        goto failed;
    }

    memcpy(context->track2, track.data, (size_t)track.len);

    context->track2Len                  = (size_t)track.len;
    context->track2[context->track2Len] = '\0';

    /* Populate the existing common transaction PAN. */
    memcpy(txn->core.pan, context->pan, panLen + 1u);

    context->ready = true;

    LOG_TRACE("MagCard: captured successfully PAN length=%lu "
              "Track2 length=%lu",
              (unsigned long)panLen, (unsigned long)context->track2Len);

    return true;

failed:
    magCardTxnClear(context);
    return false;
}