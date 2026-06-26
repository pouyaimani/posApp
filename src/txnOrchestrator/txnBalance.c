#include "txnFLow.h"
#include "settings/settings.h"

static void balanceDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st);
}

const TxnFlowConfig balanceTxn = {

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_BALANCE,

    .build = buildCommon,

    .parse = parseCommon,

    .done = balanceDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};