#include "services.h"
#include "states/states.h"
#include "dev/dev.h"
#include "ui/ui.h"
#include "msg.h"

static SubState *enterAmount;
static SubState *enterPass;
static SubState *commu;
static SubState *result;

STATE_DEF_ENTER(Sale) {
    SM_GOTO(enterAmount);
    // packer()->reset();
}

/******************** Enter amount sub state **********************/

static char *amount;

STATE_DEF_ENTER(EnterAmount) {
    GOTO_INPUT(STATE_IDLE, enterPass,
        "مبلغ", "", AMOUNT_MAX_CNT, IN_MODE_AMOUNT);
}

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {
    GOTO_INPUT(STATE_IDLE, commu,
        "رمز کارت", "", PASSWORD_MAX_LEN, IN_MODE_PASSWORD);
    // Set packager amount before reseting input
    // OOP_CALL(packer(), setAmount, in->input);
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Communication) {
    GOTO_COMMU(STATE_IDLE, result);
}

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) {
    GOTO_TXN_RES(STATE_IDLE, STATE_IDLE);
}

/******************************************************************/


OOP_CTOR(Sale, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Sale);

    enterAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, &self->base.state, "enter Amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterAmount);

    enterPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &self->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    commu = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, commu, &self->base.state, "communication");
    commu->vtable.enter = STATE_ENTER(Communication);

    result = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &self->base.state, "result");
    result->vtable.enter = STATE_ENTER(Result);
}