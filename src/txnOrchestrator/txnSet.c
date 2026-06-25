#include "txnFlow.h"
#include "isoBuilder.h"

static int buildSettlement(TxnCore* txn, ByteArray* ba) {
    return isoBuild(MTI_SETTLEMENT, NULL, ba);
}

static int parseSettlement(ByteArray* ba) {
    return isoParse(MTI_SETTLEMENT, ba);
}

const TxnFlowConfig settlementTxn = {

    .build = buildSettlement,

    .parse = parseSettlement,

    .done = commonDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};