#include "txnFLow.h"
#include "settings/settings.h"
#include "sys/sys.h"

static void logOnDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st);
}

const TxnFlowConfig logOnTxn = {

    .mti = MTI_NET_REQ,

    .prcode = PRC_LOG_ON,

    .build = buildCommon,

    .parse = parseCommon,

    .done = logOnDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};