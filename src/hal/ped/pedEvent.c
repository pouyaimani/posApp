#include "pedEvent.h"
#include "eventFactory.h"
#include "eventIds.h"
#include "state.h"

/* Generate the Ped factory. */
EVENT_DEFINE_FACTORY(PedEvent);

static void ped_dispatchTo(Event* self, State* state) {
    /*
     * Ped handling will be discussed below.
     * This assumes State exposes handlePed.
     */
    OOP_CALL(state, handlePed, self);
}

OOP_CTOR(PedEvent) { self->base.vtable.dispatchTo = ped_dispatchTo; }

void pedEventRegister(void) { EVENT_REGISTER(SM_EVENT_PED, PedEvent); }