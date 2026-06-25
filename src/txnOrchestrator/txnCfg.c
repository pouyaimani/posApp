#include "txnFLow.h"
#include "settings/settings.h"

static int buildLogOn(TxnCore* txn, ByteArray* ba) {
    return isoBuild(MTI_CFG, txn, ba);
}

static int parseLogOn(TxnCore* txn, ByteArray* ba) {
    return isoParse(MTI_CFG, txn, ba);
}

static void cfgDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st);
}

const TxnFlowConfig cfgTxn = {

    .build = buildLogOn,

    .parse = parseLogOn,

    .done = cfgDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};