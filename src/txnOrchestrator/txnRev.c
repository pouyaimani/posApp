#include "txnFlow.h"

typedef struct {

    uint32_t stan;

    uint64_t amount;

    char rrn[16];

} ReverseCtx;

static int buildReverse(ByteArray* ba) { return isoBuild(MTI_REVERSAL, ba); }

static int parseReverse(ByteArray* ba) { return isoParse(MTI_REVERSAL, ba); }

const TxnFlowConfig reverseTxn = {

    .build = buildReverse,

    .parse = parseReverse,

    .done = commonDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};