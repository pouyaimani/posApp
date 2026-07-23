#include "txnFlow.h"
#include <string.h>
#include "error.h"
#include "logger.h"
#include "txnPendingMgr.h"

static int8_t onConnect(NthTransaction* tx, void* ctx);

static int8_t onSent(NthTransaction* tx, void* ctx);

static int8_t onReceive(NthTransaction* tx, void* ctx);

static int8_t onFailure(NthTransaction* tx, void* ctx);

static int8_t onTimeout(NthTransaction* tx, void* ctx);

static int8_t isComplete(NthTransaction* tx, void* ctx);

void txnFlowRelease(TxnFlow* flow) {
    RETURN_VALUE_IF_NULL(flow, ;, false);
    LOG_DEBUG("Txn flow: rleasing flow ...");
    if (flow && flow->tx) {
        nth()->release(flow->tx);
        flow->tx = NULL;
    }
    flow->stage = TXN_STAGE_IDLE;
}

static void complete(TxnFlow* flow, TxnFlowResult result, int code) {
    RETURN_VALUE_IF_NULL(flow, ;, false);
    LOG_DEBUG("Txn flow: complete. result = %d, code = %d", result, code);
    TxnFlowStatus st;

    st.result = result;
    st.stage  = flow->stage;
    st.code   = code;

    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");
    if (code == 0) {
        LOG_TRACE("Txn flow: txn is approved. settlment need = %d",
                  flow->cfg->needSettlement);
        txnPendingMgr()->approve(&flow->data, flow->cfg->needSettlement);
    } else {
        LOG_TRACE("Txn flow: txn is declined.");
        txnPendingMgr()->decline(&flow->data);
    }
    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");
    LOG_DEBUG("-----------------------------------");

    if (flow->cfg && flow->cfg->done) {
        LOG_DEBUG("Txn flow: calling config done.");
        flow->cfg->done(flow, &st);
    }
    txnFlowRelease(flow);
}

void txnFlowInit(TxnFlow* flow) {
    RETURN_VALUE_IF_NULL(flow, ;, false);
    LOG_DEBUG("Txn flow: initing flow");
    memset(flow, 0, sizeof(*flow));
    flow->stage = TXN_STAGE_IDLE;
}

bool txnRun(TxnFlow* flow, State* owner, const char* host, uint16_t port,
            const TxnFlowConfig* cfg) {

    RETURN_VALUE_IF_NULL(flow, ;, false);
    RETURN_VALUE_IF_NULL(owner, ;, false);
    RETURN_VALUE_IF_NULL(host, ;, false);
    LOG_DEBUG("Txn flow: running flow ..., host = %s, port = %d", host, port);

    txnFlowInit(flow);

    flow->owner = owner;

    flow->cfg = cfg;

    flow->stage = TXN_STAGE_CONNECTING;

    flow->tx = nth()->alloc();

    if (!flow->tx) {
        complete(flow, TXN_FLOW_FAILED, NTH_ERR_NO_MEMORY);
        return false;
    }

    flow->tx->owner = owner;

    flow->tx->userData = flow;

    flow->tx->onConnect = onConnect;

    flow->tx->onSent = onSent;

    flow->tx->onReceive = onReceive;

    flow->tx->onFailure = onFailure;

    flow->tx->onTimeout = onTimeout;

    flow->tx->isComplete = isComplete;

    if (cfg && cfg->onConnecting) {
        cfg->onConnecting(flow);
    }
    NthResult res = nth()->connect(flow->tx, host, port);
    if (res != NTH_OK) {
        complete(flow, TXN_FLOW_FAILED, NTH_ERR_CONNECT);
        return false;
    }
}

static int8_t onConnect(NthTransaction* tx, void* ctx) {
    RETURN_VALUE_IF_NULL(tx, ;, false);
    LOG_DEBUG("Txn flow: on connect ...");
    TxnFlow* flow = ctx;
    ByteArray(ba, NT_TX_BUFFER_SIZE);

    /* Compose transaction related data */
    if (flow->cfg->compose) {
        flow->cfg->compose(&flow->data);
    }

    if (flow->cfg->build(flow, &ba) != ERR_OK) {
        complete(flow, TXN_FLOW_FAILED, NTH_ERR_INTERNAL);
        return -1;
    }

    flow->stage = TXN_STAGE_SENDING;

    if (flow->cfg->onSending) {
        flow->cfg->onSending(flow);
    }

    if (nth()->send(tx, &ba) != NTH_OK) {
        complete(flow, TXN_FLOW_FAILED, NTH_ERR_SEND);
        return -1;
    }
    return 0;
}

static int8_t onSent(NthTransaction* tx, void* ctx) {
    (void)tx;
    RETURN_VALUE_IF_NULL(tx, ;, false);
    LOG_DEBUG("Txn flow: on send ...");

    TxnFlow* flow = ctx;

    flow->stage = TXN_STAGE_RECEIVING;

    txnPendingMgr()->start(&flow->data);

    if (flow->cfg->onReceiving) {
        flow->cfg->onReceiving(flow);
    }
    return 0;
}

static int8_t onReceive(NthTransaction* tx, void* ctx) {
    RETURN_VALUE_IF_NULL(tx, ;, false);
    LOG_DEBUG("Txn flow: on receive ...");
    TxnFlow* flow = ctx;
    flow->stage   = TXN_STAGE_PARSING;
    nth()->disconnect(flow->tx);
    int rc = flow->cfg->parse(flow, &tx->rxBuffer);
    complete(flow, rc == ERR_NOK ? TXN_FLOW_FAILED : TXN_FLOW_SUCCESS, rc);
    return 0;
}

static int8_t onFailure(NthTransaction* tx, void* ctx) {
    RETURN_VALUE_IF_NULL(tx, ;, false);
    LOG_DEBUG("Txn flow: on failure ...");
    TxnFlow* flow = ctx;
    nth()->disconnect(flow->tx);
    complete(ctx, TXN_FLOW_FAILED, 0);
    return 0;
}

static int8_t onTimeout(NthTransaction* tx, void* ctx) {
    RETURN_VALUE_IF_NULL(tx, ;, false);
    LOG_DEBUG("Txn flow: on timeout ...");
    TxnFlow* flow = ctx;
    nth()->disconnect(flow->tx);
    complete(ctx, TXN_FLOW_TIMEOUT, 0);
    return 0;
}

static int8_t isComplete(NthTransaction* tx, void* ctx) {
    if (tx->rxBuffer.len < 2)
        return false;
    uint16_t len = (tx->rxBuffer.data[0] << 8) | tx->rxBuffer.data[1];

    return tx->rxBuffer.len >= len + 2;
}