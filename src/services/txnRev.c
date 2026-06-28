#include "txnOrchestrator/txnFLow.h"

typedef struct {

    uint32_t stan;

    uint64_t amount;

    char rrn[16];

} ReverseCtx;

const TxnFlowConfig reverseTxn = {

    .mti = MTI_REV_ADVICE,

    .prcode = PRC_REVERSE,

    .build = buildCommon,

    .parse = parseCommon,

    .done = commonDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};