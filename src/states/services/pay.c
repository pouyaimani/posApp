#include "services.h"
#include "states/states.h"
#include "sys/sys.h"

static SubState *enterAccountId;
static SubState *enterPayId;
static SubState *enterPass;
static SubState *communication;
static SubState *result;

STATE_DEF_ENTER(Pay) {
    SM_GOTO(enterPass);
}

STATE_DEF_EXIT(Pay) {

}
/******************** Enter account id sub state **********************/

STATE_DEF_ENTER(EnterAccountId) {

}

STATE_DEF_EXIT(EnterAccountId) {

}

static void EnterAccountId(Sale *parent) {
    enterAccountId = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAccountId, &parent->base.state, "enter account id");
    enterAccountId->vtable.enter = STATE_ENTER(EnterAccountId);
    enterAccountId->vtable.exit = STATE_EXIT(EnterAccountId);
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


OOP_CTOR(Pay, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Pay);
    self->base.state.vtable.exit = STATE_EXIT(Pay);

    EnterAccountId(self);
    EnterPayId(self);
    EnterPassword(self);
    Result(self);
}