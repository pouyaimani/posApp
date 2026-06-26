#include "txnFlow.h"
#include "isoBuilder.h"

const TxnFlowConfig settlementTxn = {

    .mti = MTI_FIN_ADVICE,

    .prcode = PRC_SETTLE,

    .build = buildCommon,

    .parse = parseCommon,

    .done = commonDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};