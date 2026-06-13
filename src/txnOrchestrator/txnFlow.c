#include "txnFlow.h"

#include <string.h>

#include "error.h"

static int8_t onConnect(
    NthTransaction *tx,
    void *ctx);

static int8_t onSent(
    NthTransaction *tx,
    void *ctx);

static int8_t onReceive(
    NthTransaction *tx,
    void *ctx);

static int8_t onFailure(
    NthTransaction *tx,
    void *ctx);

static int8_t onTimeout(
    NthTransaction *tx,
    void *ctx);

static void complete(TxnFlow *flow,
                        TxnFlowResult result, int code) {
    TxnFlowStatus st;

    st.result = result;
    st.stage = flow->stage;
    st.code = code;

    if (flow->cfg && flow->cfg->done) {
        flow->cfg->done(flow, &st);
    }

    txnFlowRelease(flow);
}

void txnFlowInit(TxnFlow *flow) {
    memset(flow, 0, sizeof(*flow));
    flow->stage =
        TXN_STAGE_IDLE;
}

bool txnRun(
    TxnFlow *flow,
        State *owner,
            const char *host,
            uint16_t port,
                const TxnFlowConfig *cfg) {

    txnFlowInit(flow);

    flow->owner = owner;

    flow->cfg = cfg;

    flow->stage =TXN_STAGE_CONNECTING;

    flow->tx = nth()->alloc();

    if (!flow->tx) {
        complete(flow, TXN_FLOW_FAILED,
                    NTH_ERR_NO_MEMORY);
        return false;
    }

    flow->tx->owner = owner;

    flow->tx->userData = flow;

    flow->tx->onConnect = onConnect;

    flow->tx->onSent = onSent;

    flow->tx->onReceive = onReceive;

    flow->tx->onFailure = onFailure;

    flow->tx->onTimeout = onTimeout;

    if (cfg && cfg->onConnecting) {
        cfg->onConnecting(flow);
    }

    NthResult res = nth()->connect(flow->tx,
                        host,port);
    if (res != NTH_OK) {
        complete(flow, TXN_FLOW_FAILED,
                    NTH_ERR_CONNECT);
        return false;
    }
}

void txnFlowRelease(TxnFlow *flow) {
    if (flow && flow->tx) {
        nth()->release(flow->tx);
        flow->tx = NULL;
    }
    flow->stage =
        TXN_STAGE_IDLE;
}

static int8_t onConnect(NthTransaction *tx, void *ctx) {
    TxnFlow *flow = ctx;
    ByteArray(ba, NT_TX_BUFFER_SIZE);
    if (flow->cfg->build(&ba) != ERR_OK) {
        complete(flow, TXN_FLOW_FAILED,
                    NTH_ERR_INTERNAL);
        return -1;
    }

    flow->stage = TXN_STAGE_SENDING;

    if (flow->cfg->onSending) {
        flow->cfg->onSending(flow);
    }

    if (nth()->send(tx, &ba) != NTH_OK) {
        complete(flow, TXN_FLOW_FAILED,
                    NTH_ERR_SEND);
        return -1;
    }
    return 0;
}

static int8_t onSent(
    NthTransaction *tx,
    void *ctx) {
    (void)tx;

    TxnFlow *flow =
        ctx;

    flow->stage =
        TXN_STAGE_RECEIVING;

    if (flow->cfg->onReceiving) {
        flow->cfg->onReceiving(flow);
    }
    return 0;
}

static int8_t onReceive(NthTransaction *tx,
                            void *ctx) {
    TxnFlow *flow = ctx;
    flow->stage =
        TXN_STAGE_PARSING;
    int rc = flow->cfg->parse(&tx->rxBuffer);
    complete(flow, rc == ERR_NOK ?
            TXN_FLOW_FAILED : TXN_FLOW_SUCCESS,
                rc);
    return 0;
}

static int8_t onFailure(NthTransaction *tx, void *ctx) {
    (void)tx;
    complete(ctx, TXN_FLOW_FAILED, 0);
    return 0;
}

static int8_t onTimeout(NthTransaction *tx, void *ctx) {
    (void)tx;
    complete(ctx, TXN_FLOW_TIMEOUT, 0);
    return 0;
}