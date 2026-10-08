#include "celEvent.h"
#include "eventFactory.h"
#include "eventIds.h"
#include "state.h"

/* Generate the Cellular factory. */
EVENT_DEFINE_FACTORY(CellEvent);

static void cel_dispatchTo(Event* self, State* state) {
    /*
     * Cellular handling will be discussed below.
     * This assumes State exposes handleCell.
     */
    OOP_CALL(state, handleCell, self);
}

OOP_CTOR(CellEvent) { self->base.vtable.dispatchTo = cel_dispatchTo; }

void celEventRegister(void) { EVENT_REGISTER(SM_EVENT_CELLULAR, CellEvent); }