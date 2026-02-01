#include "services.h"
#include "states/states.h"

SubState *enterAmount;
SubState *enterPass;
SubState *communication;
SubState *result;

STATE_DEF_ENTER(Sale) {
    SM_GOTO(enterAmount);
}

STATE_DEF_EXIT(Sale) {

}

/******************** Enter amount sub state **********************/

STATE_DEF_ENTER(EnterAmount) {
    Input * in = (Input*)getState(STATE_ID_INPUT);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, getState(STATE_ID_IDLE));
    OOP_CALL(getState(STATE_ID_INPUT), setNext, enterPass);
    in->setMode(IN_MODE_AMOUNT);
    SM_GOTO(getState(STATE_ID_INPUT));
}

STATE_DEF_EXIT(EnterAmount) {

}

static void EnterAmount(Sale *parent) {
    enterAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, &parent->base.state, "enter Amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterAmount);
    enterAmount->vtable.exit = STATE_EXIT(EnterAmount);
}

/******************************************************************/

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {

}

STATE_DEF_EXIT(EnterPassword) {

}

static void EnterPassword(Sale *parent) {
    enterPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &parent->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);
    enterPass->vtable.exit = STATE_EXIT(EnterPassword);
}

/******************************************************************/

/******************** Communication sub state **********************/

STATE_DEF_ENTER(Communication) {

}

STATE_DEF_EXIT(Communication) {

}

static void Communication(Sale *parent) {
    communication = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, communication, &parent->base.state, "Communication");
    communication->vtable.enter = STATE_ENTER(Communication);
    communication->vtable.exit = STATE_EXIT(Communication);
}

/******************************************************************/

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) {

}

STATE_DEF_EXIT(Result) {

}

static void Result(Sale *parent) {
    result = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &parent->base.state, "Result");
    result->vtable.enter = STATE_ENTER(Result);
    result->vtable.exit = STATE_EXIT(Result);
}

/******************************************************************/


OOP_CTOR(Sale, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Sale);
    self->base.state.vtable.exit = STATE_EXIT(Sale);

    EnterAmount(self);
    EnterPassword(self);
    Communication(self);
    Result(self);
}