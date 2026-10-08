#include "wifiEvent.h"
#include "eventFactory.h"
#include "eventIds.h"
#include "state.h"

/* Generate the Wifi factory. */
EVENT_DEFINE_FACTORY(WifiEvent);

static void wifi_dispatchTo(Event* self, State* state) {
    /*
     * WIFI handling will be discussed below.
     * This assumes State exposes handleWifi.
     */
    OOP_CALL(state, handleWifi, self);
}

OOP_CTOR(WifiEvent) { self->base.vtable.dispatchTo = wifi_dispatchTo; }

void wifiEventRegister(void) { EVENT_REGISTER(SM_EVENT_WIFI, WifiEvent); }