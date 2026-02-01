#include "services.h"
#include "states/states.h"
#include "dev/dev.h"

SubState *selectOperator;
SubState *selectAmount;
SubState *enterPass;
SubState *communication;
SubState *result;

STATE_DEF_ENTER(SimCharge) {
    SM_GOTO(enterPass);
}

STATE_DEF_EXIT(SimCharge) {

}
/******************** Select Operator sub state **********************/

STATE_DEF_ENTER(SelectOperator) {

}

STATE_DEF_EXIT(SelectOperator) {

}

static void SelectOperator(Sale *parent) {
    selectOperator = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, selectOperator, &parent->base.state, "select operator");
    selectOperator->vtable.enter = STATE_ENTER(SelectOperator);
    selectOperator->vtable.exit = STATE_EXIT(SelectOperator);
}

/******************************************************************/

/******************** Select amount sub state **********************/

STATE_DEF_ENTER(SelectAmount) {

}

STATE_DEF_EXIT(SelectAmount) {

}

static void SelectAmount(Sale *parent) {
    selectAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, selectAmount, &parent->base.state, "select amount");
    selectAmount->vtable.enter = STATE_ENTER(SelectAmount);
    selectAmount->vtable.exit = STATE_EXIT(SelectAmount);
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


OOP_CTOR(SimCharge, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(SimCharge);
    self->base.state.vtable.exit = STATE_EXIT(SimCharge);

    SelectOperator(self);
    SelectAmount(self);
    EnterPassword(self);
    Communication(self);
    Result(self);
}