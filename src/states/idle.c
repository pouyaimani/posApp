#include "states.h"

static void enter() {

}

static void exit() {
    
}

static void handleTimeout(State *state, TimeOutEvent *ev) {

}

static void handleKeypad(State *state, KeypadEvent *ev) {
    
}

OOP_CTOR(Idle, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = enter;
    self->base.vtable.exit = exit;
    self->base.vtable.handleKeypad = handleKeypad;
    self->base.vtable.handleTimeout = handleTimeout;
}