#include "services.h"
#include "states/states.h"
#include "dev/dev.h"

static SubState *enterBillId;
static SubState *enterPayId;
static SubState *enterPass;
static SubState *communication;
static SubState *result;

STATE_DEF_ENTER(Bill) {
    SM_GOTO(enterPass);
}

STATE_DEF_EXIT(Bill) {

}
/******************** Enter Bill id sub state **********************/

STATE_DEF_ENTER(EnterBillId) {

}

STATE_DEF_EXIT(EnterBillId) {

}

static void EnterBillId(Sale *parent) {
    enterBillId = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterBillId, &parent->base.state, "enter bill id");
    enterBillId->vtable.enter = STATE_ENTER(EnterBillId);
    enterBillId->vtable.exit = STATE_EXIT(EnterBillId);
}

/******************************************************************/

/******************** Enter pay id sub state **********************/

STATE_DEF_ENTER(EnterPayId) {

}

STATE_DEF_EXIT(EnterPayId) {

}

static void EnterPayId(Sale *parent) {
    enterPayId = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPayId, &parent->base.state, "enter pay id");
    enterPayId->vtable.enter = STATE_ENTER(EnterPayId);
    enterPayId->vtable.exit = STATE_EXIT(EnterPayId);
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


OOP_CTOR(Bill, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Bill);
    self->base.state.vtable.exit = STATE_EXIT(Bill);

    EnterBillId(self);
    EnterPayId(self);
    EnterPassword(self);
    Communication(self);
    Result(self);
}