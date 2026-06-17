#include "txnFLow.h"
#include "states/states.h"
#include "ui/infoPage.h"
#include "txn.h"
#include "byteArray.h"
#include "phrases/phrases.h"

static int buildLogOn(
    ByteArray *ba) {
    return isoBuild(MTI_CFG, ba);
}

static int parseLogOn(
    ByteArray *ba) {
    return isoParse(MTI_CFG, ba);
}

const TxnFlowConfig cfgTxn = {

    .build =
        buildLogOn,

    .parse =
        parseLogOn,

    .done =
        commonDone,

    .onConnecting =
        showConnecting,

    .onSending =
        showSending,

    .onReceiving =
        showReceiving
};