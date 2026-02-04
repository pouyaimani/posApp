#include "states.h"

static Startup startup;
static Idle idle;
static Input input;
static CardHolder cardHolder;
static Info info;

State *getState(StateId_t id) {
    switch (id) {
    case STATE_ID_START_UP:
        CALL_ONCE(OOP_CALL_CTOR(Startup, &startup, NULL, "startup"););
        return (State *)&startup;
    case STATE_ID_IDLE:
        CALL_ONCE(OOP_CALL_CTOR(Idle, &idle, &startup, "idle"););
        return (State *)&idle;
    case STATE_ID_INPUT:
        CALL_ONCE(OOP_CALL_CTOR(Input, &input, NULL, "input"););
        return (State *)&input;
    case STATE_ID_CARD_HOLDER:
        CALL_ONCE(OOP_CALL_CTOR(CardHolder, &cardHolder, &idle, "card holder"););
        return (State *)&cardHolder;
    case STATE_ID_INFO:
        CALL_ONCE(OOP_CALL_CTOR(Info, &info, &idle, "info"););
        return (State *)&info;
    default:
        break;
    }
}