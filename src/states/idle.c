#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "dev.h"

STATE_DEF_ENTER() {
    getEventloop()->registerChecker(getMagReader()->ioRead);
}

STATE_DEF_EXIT() {
    getEventloop()->unregisterChecker(getMagReader()->ioRead);
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