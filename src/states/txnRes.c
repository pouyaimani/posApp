#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "display.h"
#include "event.h"
#include "eventloop.h"
#include "receipt/receiptTemplates.h"
#include "txnOrchestrator/txnPendingMgr.h"

/******************** txn result state **********************/

static int pendMgrDone() {
    LOG_DEBUG("Pending manager is done.");
    SM_GOTO(STATE_IDLE);
}

STATE_DEF_ENTER(TxnResult) {
    TxnResult* self = (TxnResult*)state;
    showDigitalRec(&self->data);
}

STATE_DEF_HANDLE(TxnResult, KeypadEvent) {
    if (ev->key == KEY_ENTER) {
    } else if (ev->key == KEY_ESC) {
    }
    hideDigitalRec();
    txnPendingMgr()->run(pendMgrDone);
}

STATE_DEF_HANDLE(TxnResult, TimeOutEvent) {}

OOP_CTOR(TxnResult, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(TxnResult);
    self->base.vtable.handleTimeout = STATE_HANDLE(TxnResult, TimeOutEvent);
    self->base.vtable.handleKeypad  = STATE_HANDLE(TxnResult, KeypadEvent);
}