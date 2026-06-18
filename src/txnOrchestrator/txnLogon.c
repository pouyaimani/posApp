#include "txnFLow.h"
#include "states/states.h"
#include "ui/infoPage.h"
#include "txn.h"
#include "byteArray.h"
#include "phrases/phrases.h"

static int buildLogOn(ByteArray* ba) { return isoBuild(MTI_LOG_ON, ba); }

static int parseLogOn(ByteArray* ba) { return isoParse(MTI_LOG_ON, ba); }

const TxnFlowConfig logOnTxn = {

    .build = buildLogOn,

    .parse = parseLogOn,

    .done = commonDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};