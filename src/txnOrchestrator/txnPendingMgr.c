#include "txnPendingMgr.h"
#include "storage/storage.h"
#include "txn.h"
#include "error.h"
#include "settings/settings.h"
#include "utility/alphabetic.h"

#define RECOVERY_TXN_DATA_ADDR "/mtd0/txn_rec"

extern const TxnFlowConfig settlementTxn;
extern const TxnFlowConfig reverseTxn;

static TxnPendingMgr __txnPendingMgr;
static TxnData       pendTxnData;

static TxnFlow flow;

static const DataDescriptor pendingTxnDsc[] = {
    {"txn", T_BINARY, (0), (sizeof(TxnData)), ("0"), &(pendTxnData)}};

static Error_t loadTxnData(TxnData* data) {
    Error_t err = storage()->load(
        pendingTxnDsc, sizeof(pendingTxnDsc) / sizeof(DataDescriptor),
        RECOVERY_TXN_DATA_ADDR);
    RETURN_VALUE_IF_NOT(err, ERR_OK, ;, ERR_NOK);
    *data = pendTxnData;
    return ERR_OK;
}

static Error_t updateTxnData(TxnData* data) {
    pendTxnData = *data;
    return storage()->save(pendingTxnDsc,
                           sizeof(pendingTxnDsc) / sizeof(DataDescriptor),
                           RECOVERY_TXN_DATA_ADDR);
}

static void start(TxnData* txn, bool needSettle) {
    txn->status = TXN_STATUS_NEW;
    updateTxnData(txn);
}

static void approve(TxnData* txn, bool needSettle) {
    txn->status = TXN_STATUS_APPROVED;

    if (needSettle) {
        txn->status = TXN_STATUS_PENDING_SETTLEMENT;
    }
    updateTxnData(txn);
}

static void decline(TxnData* txn) {
    txn->status = TXN_STATUS_DECLINED;

    updateTxnData(txn);
}

static void markReverse(TxnData* txn) {
    txn->status = TXN_STATUS_PENDING_REVERSE;

    updateTxnData(txn);
}

static void markSettlement(TxnData* txn) {
    txn->status = TXN_STATUS_PENDING_SETTLEMENT;

    updateTxnData(txn);
}

static void processReverse(TxnData* tx) {

    txnFlowInit(&flow);

    flow.data = *tx;

    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(&flow, NULL, ip, settings()->server.mainServerPort, &reverseTxn);
}

static void processSettlement(TxnData* tx) {
    txnFlowInit(&flow);

    flow.data = *tx;

    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(&flow, NULL, ip, settings()->server.mainServerPort, &settlementTxn);
}

static bool getPendingReverse(TxnData* data) {
    RETURN_VALUE_IF_NOT(pendTxnData.status, TXN_STATUS_PENDING_REVERSE, ;
                        , false);
    *data = pendTxnData;
    return true;
}

static bool getPendingSettle(TxnData* data) {
    RETURN_VALUE_IF_NOT(pendTxnData.status, TXN_STATUS_PENDING_SETTLEMENT, ;
                        , false);
    *data = pendTxnData;
    return true;
}

bool hasPendingTxn() {
    if (pendTxnData.status == TXN_STATUS_PENDING_REVERSE ||
        pendTxnData.status == TXN_STATUS_PENDING_SETTLEMENT) {
        return true;
    }
    return false;
}
void run(void) {
    TxnData tx;

    if (getPendingReverse(&tx)) {
        processReverse(&tx);
    }

    if (getPendingSettle(&tx)) {
        processSettlement(&tx);
    }
}

static void init(TxnPendingMgr* mgr) {
    mgr->approve           = approve;
    mgr->decline           = decline;
    mgr->markReverse       = markReverse;
    mgr->markSettlement    = markSettlement;
    mgr->hasPendingTxn     = hasPendingTxn;
    mgr->getPendingReverse = getPendingReverse;
    mgr->getPendingSettle  = getPendingSettle;
    mgr->start             = start;
}

TxnPendingMgr* txnPendingMgr(void) {
    CALL_ONCE(init(&__txnPendingMgr););
    return &__txnPendingMgr;
}