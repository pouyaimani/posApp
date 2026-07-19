#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "display.h"
#include "event.h"
#include "eventloop.h"
#include "receipt/receiptTemplates.h"

static ReceiptData recData;

/******************** txn result state **********************/

STATE_DEF_ENTER(TxnResult) {
    TxnResult* self = (TxnResult*)state;
    showDigitalRec((const ReceiptData*)self->data);
}

STATE_DEF_HANDLE(TxnResult, KeypadEvent) {
    if (ev->key == KEY_ENTER) {
    } else if (ev->key == KEY_ESC) {
    }
    hideDigitalRec();
    SM_GOTO(STATE_IDLE);
}

STATE_DEF_HANDLE(TxnResult, TimeOutEvent) {}

OOP_CTOR(TxnResult, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(TxnResult);
    self->base.vtable.handleTimeout = STATE_HANDLE(TxnResult, TimeOutEvent);
    self->base.vtable.handleKeypad  = STATE_HANDLE(TxnResult, KeypadEvent);
    self->data                      = &recData;
}