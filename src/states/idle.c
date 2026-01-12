#include "states.h"
#include "core/eventloop/eventloop.h"
#include "hal/magReader/magReader.h"

STATE_DEF_ENTER() {
    getEventloop()->registerChecker(getMagReader()->vtable.readIo);
}

STATE_DEF_EXIT() {
    getEventloop()->unregisterChecker(getMagReader()->vtable.readIo);
}

STATE_DEF_HANDLE(TimeOutEvent) {

}

STATE_DEF_HANDLE(KeypadEvent) {

}

STATE_DEF_HANDLE(MagEvent) {

}

OOP_CTOR(Idle, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER();
    self->base.vtable.exit = STATE_EXIT();
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->base.vtable.handleMag = STATE_HANDLE(MagEvent);
}