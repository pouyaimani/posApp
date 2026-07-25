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

static TxnPendMgrCb settleDoneCb;

static const DataDescriptor pendingTxnDsc[] = {
    {"txn", T_BINARY, (0), (sizeof(TxnData)), ("0"), &(pendTxnData)}};

void settleDone(TxnFlow* flow, const TxnFlowStatus* st) {
    (void*)flow;
    (void*)st;
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
    bool res = (st->result == TXN_FLOW_SUCCESS) && (st->code == 0);
    settleDoneCb(res);
}

void reverseDone(TxnFlow* flow, const TxnFlowStatus* st) {
    (void*)flow;
    (void*)st;
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
    bool res = (st->result == TXN_FLOW_SUCCESS) && (st->code == 0);
    settleDoneCb(res);
}

static Error_t loadTxnData(TxnData* data) {
    RETURN_IF_NULL(data, ;);
    LOG_TRACE("TxnPendingMgr: loading transaction data ...");
    Error_t err = storage()->load(
        pendingTxnDsc, sizeof(pendingTxnDsc) / sizeof(DataDescriptor),
        RECOVERY_TXN_DATA_ADDR);
    RETURN_VALUE_IF_NOT(err, ERR_OK, ;, ERR_NOK);
    *data = pendTxnData;
    return ERR_OK;
}

static Error_t updateTxnData(TxnData* data) {
    RETURN_IF_NULL(data, ;);
    LOG_TRACE("TxnPendingMgr: updating transaction data ...");
    pendTxnData = *data;
    LOG_DEBUG("amount = %llu", pendTxnData.core.amount);
    LOG_DEBUG("rrn = %llu", pendTxnData.core.rrn);
    LOG_DEBUG("trace = %lu", pendTxnData.core.trace);
    return storage()->save(pendingTxnDsc,
                           sizeof(pendingTxnDsc) / sizeof(DataDescriptor),
                           RECOVERY_TXN_DATA_ADDR);
}

static void start(TxnData* txn, bool needSettle) {
    LOG_TRACE("TxnPendingMgr: transaction is started ...");
    txn->status = TXN_STATUS_NEW;
    updateTxnData(txn);
}

static void approve(TxnData* txn, bool needSettle) {
    RETURN_IF_NULL(txn, ;);
    txn->status = TXN_STATUS_APPROVED;

    if (needSettle) {
        txn->status = TXN_STATUS_PENDING_SETTLEMENT;
    }
    updateTxnData(txn);

    LOG_TRACE("TxnPendingMgr: transaction is approved ...");
}

static void decline(TxnData* txn) {
    RETURN_IF_NULL(txn, ;);

    if (txn->core.mti == MTI_FIN_ADVICE || txn->core.mti == MTI_REV_REQ) {
        return;
    }
    txn->status = TXN_STATUS_DECLINED;

    updateTxnData(txn);

    LOG_TRACE("TxnPendingMgr: transaction is declined ...");
}

static void markReverse(TxnData* txn) {
    txn->status = TXN_STATUS_PENDING_REVERSE;

    updateTxnData(txn);
}

static void markSettlement(TxnData* txn) {
    txn->status = TXN_STATUS_PENDING_SETTLEMENT;

    updateTxnData(txn);
}

static void processPendedTxn(TxnData* tx, TxnFlowConfig* cfg) {
    txnFlowInit(&flow);

    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(&flow, NULL, ip, settings()->server.mainServerPort, cfg);
    flow.data = *tx;
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
    settleDoneCb = cb;
    if (isReverse(&tx)) {
        LOG_TRACE("TxnPendingMgr: a reverse txn exists.");
        processReverse(&tx);
    } else if (isSettle(&tx)) {
        LOG_TRACE("TxnPendingMgr: a settlement txn exists.");
        processSettlement(&tx);
    }
    return true;
}

static void init(TxnPendingMgr* mgr) {
    mgr->approve        = approve;
    mgr->decline        = decline;
    mgr->markReverse    = markReverse;
    mgr->markSettlement = markSettlement;
    mgr->hasPendingTxn  = hasPendingTxn;
    mgr->start          = start;
    mgr->run            = run;
}

TxnPendingMgr* txnPendingMgr(void) {
    CALL_ONCE(init(&__txnPendingMgr););
    return &__txnPendingMgr;
}