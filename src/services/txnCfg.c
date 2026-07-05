#include "txnOrchestrator/txnFLow.h"
#include "settings/settings.h"

static void cfgDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st, flow->owner->parent, flow->owner->parent, true);
}

const TxnFlowConfig cfgTxn = {

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_CFG,

    .build = buildCommon,

    .parse = parseCommon,

    .done = cfgDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};