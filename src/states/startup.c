#include "states.h"
#include "logger.h"
#include "event.h"
#include "dev/dev.h"
#include "display.h"

STATE_DEF_ENTER(Startup) {

}

STATE_DEF_EXIT(Startup) {

}

STATE_DEF_HANDLE(TimeOutEvent) {

}

STATE_DEF_HANDLE(KeypadEvent) {
        LOG_TRACE("event successfully is reached. key = %d", ev->key);
    if (ev->key == KEY_ESC)
        OOP_CALL(getDevice(), powerOff);
}

OOP_CTOR(Startup, State *parent, const char *name) {
    LOG_TRACE("Constructing Startup ...");
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Startup);
    self->base.vtable.exit = STATE_EXIT(Startup);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
}