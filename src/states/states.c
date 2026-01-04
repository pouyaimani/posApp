#include "states.h"

static Startup startup;
static Idle idle;

State *getState(StateId_t id) {
    switch (id) {
    case STATE_ID_START_UP:
        CALL_ONCE(
            Startup_ctor(&startup, NULL, "startup");
        );
        return (State *)&startup;

    case STATE_ID_IDLE:
        CALL_ONCE(
            Idle_ctor(&idle, &startup, "idle");
        );
        return (State *)&idle;
    
    default:
        break;
    }
}