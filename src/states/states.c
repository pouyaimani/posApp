#include "states.h"

static Startup startup;
static Idle idle;
static Input input;
static CardHolder cardHolder;

State *getState(StateId_t id) {
    switch (id) {
    case STATE_ID_START_UP:
        CALL_ONCE(Startup_ctor(&startup, NULL, "startup"););
        return (State *)&startup;
    case STATE_ID_IDLE:
        CALL_ONCE(Idle_ctor(&idle, &startup, "idle"););
        return (State *)&idle;
    case STATE_ID_INPUT:
        CALL_ONCE(Input_ctor(&input, NULL, "input"););
        return (State *)&input;
    case STATE_ID_CARD_HOLDER:
        CALL_ONCE(CardHolder_ctor(&cardHolder, &idle, "card holder"););
        return (State *)&cardHolder;
    default:
        break;
    }
}