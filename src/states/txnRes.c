#include "states.h"
#include "logger.h"
#include "dev.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "eventloop.h"

/******************** txn result state **********************/

STATE_DEF_ENTER(TxnResult) {
}

OOP_CTOR(TxnResult, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(TxnResult);

}