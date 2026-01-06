#include "states.h"
#include "core/eventloop/eventloop.h"
#include "hal/magReader/magReader.h"

static void enter() {
    getEventloop()->registerChecker(getMagReader()->vtable.readIo);
}

static void exit() {
    getEventloop()->unregisterChecker(getMagReader()->vtable.readIo);
}

static void handleTimeout(State *state, TimeOutEvent *ev) {

}

static void handleKeypad(State *state, KeypadEvent *ev) {
    
}

static void handleMag(State *state, MagEvent *ev) {
    
}

OOP_CTOR(Idle, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = enter;
    self->base.vtable.exit = exit;
    self->base.vtable.handleKeypad = handleKeypad;
    self->base.vtable.handleTimeout = handleTimeout;
    self->base.vtable.handleMag = handleMag;
}