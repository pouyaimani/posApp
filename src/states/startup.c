#include "states.h"
#include "logger.h"

static void enter() {

}

static void exit() {
    
}

static void handleTimeout(State *state, TimeOutEvent *ev) {

}

OOP_CTOR(Startup, State *parent, const char *name) {
    LOG_TRACE("Constructing Startup ...");
    State_ctor(self, parent, name);
    self->base.vtable.enter = enter;
    self->base.vtable.exit = exit;
    self->base.vtable.handleTimeout = handleTimeout;
}