#include "piccEvent.h"
#include "eventFactory.h"
#include "eventIds.h"
#include "state.h"

/* Generate the Picc factory. */
EVENT_DEFINE_FACTORY(PiccEvent);

static void picc_dispatchTo(Event* self, State* state) {
    /*
     * Picc handling will be discussed below.
     * This assumes State exposes handlePicc.
     */
    OOP_CALL(state, handlePicc, self);
}

OOP_CTOR(PiccEvent) { self->base.vtable.dispatchTo = picc_dispatchTo; }

void piccEventRegister(void) { EVENT_REGISTER(SM_EVENT_PICC, PiccEvent); }