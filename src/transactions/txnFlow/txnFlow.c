#include "txnFlow.h"
#include <string.h>
#include "error.h"
#include "logger.h"
#include "txnMgr.h"
#include "settings/settings.h"

static int8_t onConnect(NthTransaction* tx, void* ctx);

static int8_t onSent(NthTransaction* tx, void* ctx);

static int8_t onReceive(NthTransaction* tx, void* ctx);

static int8_t onFailure(NthTransaction* tx, void* ctx);

static int8_t onTimeout(NthTransaction* tx, void* ctx);

static NthRxDecision isComplete(NthTransaction* tx, void* ctx);

static void complete(TxnFlow* flow, TxnFlowResult result, int code) {
    if (!flow) {
        return;
    }

    TxnFlowStatus status = {
        .result = result, .stage = flow->stage, .code = code};

    /* Capture the callback before releasing the runtime resources. */
    TxnFlowDone done = flow->cfg ? flow->cfg->done : NULL;

    /*
     * Preserve the existing trace behavior for this change.
     * Whether failed connection attempts should consume a trace
     * number is a separate transaction-policy decision.
     */
    txnTraceInfo()->inc();

    txnFlowRelease(flow);

    if (done) {
        done(flow, &status);
    }
}

void txnFlowRelease(TxnFlow* flow) {
    if (!flow) {
        return;
    }

    if (flow->tx) {
        nth()->release(flow->tx);
        flow->tx = NULL;
    }

    flow->stage = TXN_STAGE_IDLE;
}

void txnFlowInit(TxnFlow* flow) {
    RETURN_IF_NULL(flow, ;);

    LOG_TRACE("Txn flow: initing flow");
    memset(flow, 0, sizeof(*flow));
    flow->stage = TXN_STAGE_IDLE;
}

bool txnFlowPrepare(TxnFlow* flow, State* owner, const TxnFlowConfig* cfg) {
    RETURN_VALUE_IF_NULL(flow, ;, false);
    RETURN_VALUE_IF_NULL(cfg, ;, false);
    RETURN_VALUE_IF_NULL(cfg->build, ;, false);
    RETURN_VALUE_IF_NULL(cfg->parse, ;, false);

    txnFlowInit(flow);

    flow->owner = owner;
    flow->cfg   = cfg;

    flow->data.core.mti     = cfg->mti;
    flow->data.core.txnType = cfg->type;

    return true;
}

bool txnFlowConnect(TxnFlow* flow, const char* host, uint16_t port) {
    if (!flow || !host || !flow->cfg) {
        return false;
    }

    if (!flow->cfg->build || !flow->cfg->parse) {
        return false;
    }

    /* Do not start another connection on an active flow. */
    if (flow->tx || flow->stage != TXN_STAGE_IDLE) {
        return false;
    }

    LOG_TRACE("Txn flow: connecting, host = %s, port = %u", host,
              (unsigned)port);

    flow->stage = TXN_STAGE_CONNECTING;

    flow->tx = nth()->alloc();
    if (!flow->tx) {
        complete(flow, TXN_FLOW_FAILED, NTH_ERR_NO_MEMORY);
        return false;
    }

    flow->tx->owner      = flow->owner;
    flow->tx->userData   = flow;
    flow->tx->onConnect  = onConnect;
    flow->tx->onSent     = onSent;
    flow->tx->onReceive  = onReceive;
    flow->tx->onFailure  = onFailure;
    flow->tx->onTimeout  = onTimeout;
    flow->tx->isComplete = isComplete;

    if (flow->cfg->onConnecting) {
        flow->cfg->onConnecting(flow);
    }

    NthResult res = nth()->connect(flow->tx, host, port);

    LOG_TRACE("Txn flow: connect result = %d", res);

    if (res != NTH_OK) {
        complete(flow, TXN_FLOW_FAILED, NTH_ERR_CONNECT);
        return false;
    }

    return true;
}

/*
 * Existing callers retain their current behavior:
 * initialize a transaction and immediately connect.
 */
bool txnRun(TxnFlow* flow, State* owner, const char* host, uint16_t port,
            const TxnFlowConfig* cfg) {
    if (!host) {
        return false;
    }

    if (!txnFlowPrepare(flow, owner, cfg)) {
        return false;
    }

    return txnFlowConnect(flow, host, port);
}

static int8_t onConnect(NthTransaction* tx, void* ctx) {
    TxnFlow* flow = ctx;

    if (!tx || !flow || !flow->cfg) {
        return -1;
    }

    ByteArray(ba, NT_BUFFER_SIZE);

    /*
     * Compose before building so the recovery record can contain
     * the final transaction identifiers and request data.
     */
    if (flow->cfg->compose) {
        int rc = flow->cfg->compose(&flow->data);

        if (rc != ERR_OK) {
            complete(flow, TXN_FLOW_FAILED, rc);
            return -1;
        }
    }

    int rc = flow->cfg->build(flow, &ba);

    if (rc != ERR_OK) {
        complete(flow, TXN_FLOW_FAILED, rc);
        return -1;
    }

    flow->stage = TXN_STAGE_SENDING;

    /*
     * Let the owner persist recovery data before any request bytes
     * are sent. A persistence failure prevents transmission.
     */
    if (flow->cfg->beforeSend) {
        rc = flow->cfg->beforeSend(flow);

        if (rc != ERR_OK) {
            complete(flow, TXN_FLOW_FAILED, rc);
            return -1;
        }
    }

    if (flow->cfg->onSending) {
        flow->cfg->onSending(flow);
    }

    if (nth()->send(tx, &ba) != NTH_OK) {
        /*
         * Sending can fail after partial transmission.
         * Preserve any recovery record already saved.
         */
        complete(flow, TXN_FLOW_FAILED, NTH_ERR_SEND);
        return -1;
    }

    return 0;
}

static int8_t onSent(NthTransaction* tx, void* ctx) {
    TxnFlow* flow = ctx;
    RETURN_VALUE_IF_NULL(tx, ;, false);
    RETURN_VALUE_IF_NULL(flow, ;, false);
    RETURN_VALUE_IF_NULL(flow->cfg, ;, false);

    flow->stage = TXN_STAGE_RECEIVING;

    if (flow->cfg->onReceiving) {
        flow->cfg->onReceiving(flow);
    }

    return 0;
}

static int8_t onReceive(NthTransaction* tx, void* ctx) {
    RETURN_VALUE_IF_NULL(tx, ;, false);

    TxnFlow* flow = ctx;
    flow->stage   = TXN_STAGE_PARSING;
    int rc        = flow->cfg->parse(flow, &tx->rxBuffer);
    complete(flow, rc == ERR_NOK ? TXN_FLOW_FAILED : TXN_FLOW_SUCCESS, rc);
    return 0;
}

static int8_t onFailure(NthTransaction* tx, void* ctx) {
    RETURN_VALUE_IF_NULL(tx, ;, false);
    LOG_TRACE("Txn flow: on failure ...");
    TxnFlow* flow = ctx;
    complete(ctx, TXN_FLOW_FAILED, 0);
    return 0;
}

static int8_t onTimeout(NthTransaction* tx, void* ctx) {
    RETURN_VALUE_IF_NULL(tx, ;, false);
    LOG_TRACE("Txn flow: on timeout ...");
    TxnFlow* flow = ctx;
    complete(ctx, TXN_FLOW_TIMEOUT, 0);
    return 0;
}

static NthRxDecision isComplete(NthTransaction* tx, void* ctx) {
    if (tx->rxBuffer.len < 2)
        return false;
    uint16_t len = (tx->rxBuffer.data[0] << 8) | tx->rxBuffer.data[1];
    return tx->rxBuffer.len >= len + 2 ? NTH_RX_COMPLETE : NTH_RX_MORE;
}