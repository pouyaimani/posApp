#include "txnFLow.h"
#include "settings/settings.h"
#include "sys/sys.h"

static int buildLogOn(TxnCore* txn, ByteArray* ba) {
    return isoBuild(MTI_LOG_ON, txn, ba);
}

static int parseLogOn(TxnCore* txn, ByteArray* ba) {
    return isoParse(MTI_LOG_ON, txn, ba);
}

static void logOnDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st);
}

const TxnFlowConfig logOnTxn = {

    .build = buildLogOn,

    .parse = parseLogOn,

    .done = logOnDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};