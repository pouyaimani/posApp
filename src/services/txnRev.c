#include "txnOrchestrator/txnFLow.h"

typedef struct {

    uint32_t stan;

    uint64_t amount;

    char rrn[16];

} ReverseCtx;

int buildReverse(TxnFlow* flow, ByteArray* ba) {
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, &flow->data, ba);
}

const TxnFlowConfig reverseTxn = {

    .mti = MTI_REV_ADVICE,

    .prcode = PRC_REVERSE,

    .build = buildReverse,

    .parse = parseCommon,

    .done = commonDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};