#include "keypadEvent.h"
#include "eventFactory.h"
#include "eventIds.h"
#include "state.h"

/* Generate the Keypad factory. */
EVENT_DEFINE_FACTORY(KeypadEvent);

static void keypad_dispatchTo(Event* self, State* state) {
    /*
     * Keypad handling will be discussed below.
     * This assumes State exposes handleKeypad.
     */
    OOP_CALL(state, handleKeypad, self);
}

OOP_CTOR(KeypadEvent) { self->base.vtable.dispatchTo = keypad_dispatchTo; }

void keypadEventRegister(void) { EVENT_REGISTER(SM_EVENT_KEYPAD, KeypadEvent); }