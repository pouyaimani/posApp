#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "display.h"
#include "event.h"
#include "eventloop.h"
#include "receipt/receiptTemplates.h"
#include "txnFlow/txnPendingMgr.h"
#include "record/txnRecs.h"

/******************** txn result state **********************/

static int pendMgrDone(const TxnData* data) {
    if (data->status == TXN_STATUS_APPROVED) {
        LOG_TRACE("Txn pending manager: transaction is approved.");
        // Result_t res = txnrecord()->insert(data);
        // if (res.err != ERR_DSC_OK) {
        //     LOG_ERROR("Inserting transaction to database is failed. error =
        //     %d "
        //               "detail code = %d",
        //               res.err, res.detail.db);
        // } else {
        //     LOG_TRACE(
        //         "Inserting transaction to database is successfully done.");
        // }
    } else if (data->status == TXN_STATUS_REVERSED) {
        LOG_TRACE("Txn pending manager: transaction is reversed.");
    } else {
        LOG_TRACE(
            "Pending manager is failed to determine transaction status (%d)",
            data->status);
    }
    SM_GOTO(STATE_IDLE);
}

STATE_DEF_ENTER(TxnResult) {
    TxnResult* self = (TxnResult*)state;
    showDigitalRec(&self->data);
}

static int8_t printResult(TxnData* txn) {
    ReceiptData data;
    data.txn           = *txn;
    data.type          = DOC_TXN;
    data.headerApplied = true;
    Receipt  rec;
    Result_t res = buildReceipt(&rec, &data);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    res = OOP_CALL(&rec, flush);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    OOP_CALL(&rec, destroy);
    return true;
}

STATE_DEF_HANDLE(TxnResult, KeypadEvent) {
    if (ev->key != KEY_ENTER && ev->key != KEY_ESC) {
        return;
    }
    TxnResult* self = (TxnResult*)state;
    if (ev->key == KEY_ENTER) {
        printResult(&self->data);
    }
    hideDigitalRec();
    if (!txnPendingMgr()->run(pendMgrDone)) {
        SM_GOTO(STATE_IDLE);
    }
}

STATE_DEF_HANDLE(TxnResult, TimeOutEvent) {}

OOP_CTOR(TxnResult, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(TxnResult);
    self->base.vtable.handleTimeout = STATE_HANDLE(TxnResult, TimeOutEvent);
    self->base.vtable.handleKeypad  = STATE_HANDLE(TxnResult, KeypadEvent);
}