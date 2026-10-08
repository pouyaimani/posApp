#include "magEvent.h"
#include "eventFactory.h"
#include "eventIds.h"
#include "state.h"

/* Generate the mag factory. */
EVENT_DEFINE_FACTORY(MagEvent);

static void mag_dispatchTo(Event* self, State* state) {
    /*
     * Magreader handling will be discussed below.
     * This assumes State exposes handleMag.
     */
    OOP_CALL(state, handleMag, self);
}

OOP_CTOR(MagEvent) { self->base.vtable.dispatchTo = mag_dispatchTo; }

void magEventRegister(void) { EVENT_REGISTER(SM_EVENT_MAG, MagEvent); }