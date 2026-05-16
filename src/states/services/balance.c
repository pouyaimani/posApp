#include "services.h"
#include "states/states.h"
#include "sys/sys.h"
#include "ui/ui.h"
#include "phrases/phrases.h"

static SubState *enterPass;
static SubState *commu;
static SubState *result;

#define INFO_WAGE   "کارمزد 1,800 ریال"

STATE_DEF_ENTER(Balance) {
    SM_GOTO(enterPass);
}

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {
    GOTO_INPUT(STATE_IDLE, commu, phraseGetDef(PHRASE_CARD_PIN),
        INFO_WAGE, PASSWORD_MAX_LEN, IN_MODE_PASSWORD, NULL);
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Communication) {
    GOTO_ISO_TRANSMITTER(STATE_IDLE, result);
}

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) {
    GOTO_TXN_RES(STATE_IDLE, STATE_IDLE);
}

/******************************************************************/


OOP_CTOR(Balance, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Balance);

    enterPass = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &self->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    commu = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, commu, &self->base.state, "communication");
    commu->vtable.enter = STATE_ENTER(Communication);

    result = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &self->base.state, "result");
    result->vtable.enter = STATE_ENTER(Result);
}