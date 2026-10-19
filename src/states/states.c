#include "states.h"
#include "sys/sys.h"
#include "supporter.h"

static Startup*     startup;
static Idle*        idle;
static CardHolder*  cardHolder;
static Dialogue*    dialogue;
static Supporter*   supporter;
static FixedAmount* fixedAmount;

State* getState(StateId_t id) {
    switch (id) {
    case STATE_ID_START_UP:
        CALL_ONCE(startup = MEM_ALLOC(sizeof(Startup));
                  OOP_CALL_CTOR(Startup, startup, NULL, "startup"););
        return (State*)startup;
    case STATE_ID_IDLE:
        CALL_ONCE(idle = MEM_ALLOC(sizeof(Idle));
                  OOP_CALL_CTOR(Idle, idle, startup, "idle"););
        return (State*)idle;
    case STATE_ID_CARD_HOLDER:
        CALL_ONCE(cardHolder = MEM_ALLOC(sizeof(CardHolder));
                  OOP_CALL_CTOR(CardHolder, cardHolder, idle, "card holder"););
        return (State*)cardHolder;
    case STATE_ID_DIALOGUE:
        CALL_ONCE(dialogue = MEM_ALLOC(sizeof(Dialogue));
                  OOP_CALL_CTOR(Dialogue, dialogue, idle, "dialogue"););
        return (State*)dialogue;
    case STATE_ID_SUPPORTER:
        CALL_ONCE(supporter = MEM_ALLOC(sizeof(Supporter));
                  OOP_CALL_CTOR(Supporter, supporter, idle, "supporter"););
        return (State*)supporter;
    case STATE_ID_FIXED_AMOUNT:
        CALL_ONCE(fixedAmount = MEM_ALLOC(sizeof(FixedAmount)); OOP_CALL_CTOR(
                      FixedAmount, fixedAmount, idle, "fixed Amount"););
        return (State*)fixedAmount;
    default:
        break;
    }
}

void GOTO_DIAL(State* prev, State* next, const char* title, const char* body,
               CallBack_t cb, void* data) {
    OOP_CALL(getState(STATE_ID_DIALOGUE), setNext, next);
    OOP_CALL(getState(STATE_ID_DIALOGUE), setPrev, prev);
    Dialogue* dial = (Dialogue*)getState(STATE_ID_DIALOGUE);
    dial->setTitle(title);
    dial->setBody(body);
    dial->cb     = cb;
    dial->cbData = data;
    SM_GOTO(getState(STATE_ID_DIALOGUE));
}