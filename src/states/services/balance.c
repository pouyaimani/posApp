#include "services.h"
#include "states/states.h"
#include "dev/dev.h"

static SubState *enterPass;
static SubState *communication;
static SubState *result;

STATE_DEF_ENTER(Balance) {

}

STATE_DEF_EXIT(Balance) {

}

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


OOP_CTOR(Balance, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Balance);
    self->base.state.vtable.exit = STATE_EXIT(Balance);

    EnterPassword(self);
    Communication(self);
    Result(self);
}