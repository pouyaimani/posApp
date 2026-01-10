#include "states.h"
#include "logger.h"
#include "core/stateMachine/event.h"
#include "hal/dev/dev.h"

static void enter() {
    LOG_TRACE("Entrting to Startup state...");
}

static void exit() {
    
}

static void handleTimeout(State *state, TimeOutEvent *ev) {

}

static void handleKeypad(State *state, KeypadEvent *ev) {
    LOG_TRACE("event successfully is reached. key = %d", ev->key);
    if (ev->key == KEY_ESC)
        OOP_CALL(getDevice(), powerOff);
}

OOP_CTOR(Startup, State *parent, const char *name) {
    LOG_TRACE("Constructing Startup ...");
    State_ctor(self, parent, name);
    self->base.vtable.enter = enter;
    self->base.vtable.exit = exit;
    self->base.vtable.handleTimeout = handleTimeout;
    self->base.vtable.handleKeypad = handleKeypad;
}