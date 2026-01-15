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
    if (ev->key == KEY_ESC)
        OOP_CALL(getDevice(), powerOff);
}

STATE_DEF_HANDLE(MagEvent) {
    LOG_TRACE("Mag event is recieved.");
    LOG_TRACE("track1 = %s", ev->data->track1);
    LOG_TRACE("track2 = %s", ev->data->track2);
    LOG_TRACE("track3 = %s", ev->data->track3);
}

OOP_CTOR(Idle, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER();
    self->base.vtable.exit = STATE_EXIT();
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->base.vtable.handleMag = STATE_HANDLE(MagEvent);
}