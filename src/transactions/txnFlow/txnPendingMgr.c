#include "txnPendingMgr.h"
#include "txnFlow.h"
#include "storage/storage.h"
#include "txn.h"
#include "error.h"
#include "settings/settings.h"
#include "utility/alphabetic.h"
#include "states.h"

#define RECOVERY_TXN_DATA_ADDR "/mtd0/txn_rec"

extern const TxnFlowConfig settlementTxn;
extern const TxnFlowConfig reverseTxn;

static TxnPendingMgr __txnPendingMgr;
static TxnData       pendTxnData;

static TxnFlow flow;

static TxnPendMgrCb pendDoneCb;

#define TAG_PENDING_TXN 0x0501

static const DataDescriptor pendingTxnDsc[] = {
    DSC_BIN_ID(TAG_PENDING_TXN, pendTxnData),
};

void settleDone(TxnFlow* flow, const TxnFlowStatus* st) {
    (void*)flow;
    (void*)st;
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
    bool res = (st->result == TXN_FLOW_SUCCESS) && (st->code == 0);
    if (res) {
        pendTxnData.status = TXN_STATUS_APPROVED;
    }
    pendDoneCb(&pendTxnData);
}

void reverseDone(TxnFlow* flow, const TxnFlowStatus* st) {
    (void*)flow;
    (void*)st;
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
    bool res = (st->result == TXN_FLOW_SUCCESS) && (st->code == 0);
    if (res) {
        pendTxnData.status = TXN_STATUS_REVERSED;
    }
    pendDoneCb(&pendTxnData);
}

static Error_t loadTxnData(TxnData* data) {
    RETURN_IF_NULL(data, ;);
    LOG_TRACE("TxnPendingMgr: loading transaction data ...");
    Error_t err = storage()->load(
        pendingTxnDsc, sizeof(pendingTxnDsc) / sizeof(DataDescriptor),
        RECOVERY_TXN_DATA_ADDR);
    RETURN_VALUE_IF_NOT(err, ERR_OK, ;, ERR_NOK);
    memcpy(data, &pendTxnData, sizeof(*data));
    return ERR_OK;
}

static Error_t updateTxnData(TxnData* data) {
    RETURN_IF_NULL(data, ;);
    LOG_TRACE("TxnPendingMgr: updating transaction data ...");
    memcpy(&pendTxnData, data, sizeof(*data));
    LOG_TRACE("amount = %llu, rrn = %llu, trace = %lu", pendTxnData.core.amount,
              pendTxnData.core.rrn, pendTxnData.core.trace);
    return storage()->save(pendingTxnDsc,
                           sizeof(pendingTxnDsc) / sizeof(DataDescriptor),
                           RECOVERY_TXN_DATA_ADDR);
}

static Error_t mark(TxnStatus st) {

    pendTxnData.status = st;
    return updateTxnData(&pendTxnData);
}

static Error_t update(TxnStatus st, TxnData* data) {
    data->status = st;
    return updateTxnData(data);
}

static Error_t new(TxnData* data) {
    if (!data) {
        return ERR_NOK;
    }

    LOG_TRACE("TxnPendingMgr: saving pending reversal.");
    return update(TXN_STATUS_PENDING_REVERSE, data);
}

static void processPendedTxn(const TxnData* tx, const TxnFlowConfig* cfg) {
    if (!tx || !cfg) {
        return;
    }

    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));

    if (!txnFlowPrepare(&flow, NULL, cfg)) {
        LOG_FATAL("Pending transaction preparation failed.");
        return;
    }

    /* Restore the original transaction before starting communication. */
    memcpy(&flow.data, tx, sizeof(flow.data));

    /*
     * The selected recovery configuration defines the outgoing
     * operation; the remaining data comes from the original transaction.
     */
    flow.data.core.mti     = cfg->mti;
    flow.data.core.txnType = cfg->type;

    txnFlowConnect(&flow, ip, settings()->server.mainServerPort);
}

static void processReverse(TxnData* tx) {
    LOG_TRACE("TxnPendingMgr: processing reverse transaction ...");
    processPendedTxn(tx, &reverseTxn);
}

static void processSettlement(TxnData* tx) {
    LOG_TRACE("TxnPendingMgr: processing settlement transaction ...");
    processPendedTxn(tx, &settlementTxn);
}

static bool isReverse(TxnData* data) {
    RETURN_VALUE_IF_NULL(data, ;, false);
    return data->status == TXN_STATUS_PENDING_REVERSE ? true : false;
}

static bool isSettle(TxnData* data) {
    RETURN_VALUE_IF_NULL(data, ;, false);
    return data->status == TXN_STATUS_PENDING_SETTLEMENT ? true : false;
}

bool hasPendingTxn(TxnData* txn) {
    if (txn->status == TXN_STATUS_PENDING_REVERSE ||
        txn->status == TXN_STATUS_PENDING_SETTLEMENT) {
        return true;
    }
    return false;
}

bool run(TxnPendMgrCb cb) {
    TxnData tx;
    loadTxnData(&tx);
    RETURN_VALUE_IF_NOT(
        hasPendingTxn(&tx), true,
        LOG_TRACE("TxnPendingMgr: no pending transaction exists.");
        , false);
    pendDoneCb = cb;
    if (isReverse(&tx)) {
        LOG_TRACE("TxnPendingMgr: a reverse txn exists.");
        processReverse(&tx);
    } else if (isSettle(&tx)) {
        LOG_TRACE("TxnPendingMgr: a settlement txn exists.");
        processSettlement(&tx);
    }
    return true;
}

static Error_t reset() {
    memset(&pendTxnData, 0, sizeof(pendTxnData));
    return updateTxnData(&pendTxnData);
}

static void init(TxnPendingMgr* mgr) {
    mgr->mark          = mark;
    mgr->hasPendingTxn = hasPendingTxn;
    mgr->run           = run;
    mgr->reset         = reset;
    mgr->new           = new;
    mgr->update        = update;
}

TxnPendingMgr* txnPendingMgr(void) {
    CALL_ONCE(init(&__txnPendingMgr););
    return &__txnPendingMgr;
}