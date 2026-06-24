#include "txnFLow.h"

static int buildLogOn(ByteArray* ba) { return isoBuild(MTI_LOG_ON, ba); }

static int parseLogOn(ByteArray* ba) { return isoParse(MTI_LOG_ON, ba); }

const TxnFlowConfig logOnTxn = {

    .build = buildLogOn,

    .parse = parseLogOn,

    .done = commonDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};