#include "services.h"
#include "states/states.h"
#include "dev/dev.h"
#include "ui/ui.h"
#include "msg.h"

static SubState *enterAmount;
static SubState *enterPass;
static SubState *connection;
static SubState *receiveData;
static SubState *sendData;
static SubState *result;

STATE_DEF_ENTER(Sale) {
    SM_GOTO(enterAmount);
    // packer()->reset();
}

/******************** Enter amount sub state **********************/

#define AMOUNT_MAX_CNT  12

static char *amount;

STATE_DEF_ENTER(EnterAmount) {
    GOTO_INPUT(getState(STATE_ID_IDLE), enterPass,
        "مبلغ", "", AMOUNT_MAX_CNT, IN_MODE_AMOUNT);
}

static void EnterAmount(Sale *parent) {
    enterAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, &parent->base.state, "enter Amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterAmount);
}

/******************************************************************/

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {
    GOTO_INPUT(getState(STATE_ID_IDLE), connection,
        "رمز کارت", "", PASSWORD_MAX_LEN, IN_MODE_PASSWORD);
    // Set packager amount before reseting input
    // OOP_CALL(packer(), setAmount, in->input);
}

static void EnterPassword(Sale *parent) {
    enterPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &parent->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);
}

/******************************************************************/

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Connection) {
    SHOW_INFO("در حال اتصال", "");
}

STATE_DEF_EXIT(Connection) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(Connection, KeypadEvent) {
    SM_GOTO(sendData);
}

static void Connection(Sale *parent) {
    connection = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, connection, &parent->base.state, "Connection");
    connection->vtable.enter = STATE_ENTER(Connection);
    connection->vtable.exit = STATE_EXIT(Connection);
    connection->vtable.handleKeypad = STATE_HANDLE(Connection, KeypadEvent);
}

/******************************************************************/

/******************** Send data sub state **********************/

STATE_DEF_ENTER(SendData) {
    SHOW_INFO("ارسال اطلاعات", "");
}

STATE_DEF_EXIT(SendData) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(SendData, KeypadEvent) {
    SM_GOTO(receiveData);
}

static void SendData(Sale *parent) {
    sendData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, sendData, &parent->base.state, "Send Data");
    sendData->vtable.enter = STATE_ENTER(SendData);
    sendData->vtable.exit = STATE_EXIT(SendData);
    sendData->vtable.handleKeypad = STATE_HANDLE(SendData, KeypadEvent);
}

/******************************************************************/

/******************** Receive data sub state **********************/

STATE_DEF_ENTER(ReceiveData) {
    SHOW_INFO("دریافت اطلاعات", "");
}

STATE_DEF_EXIT(ReceiveData) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(ReceiveData, KeypadEvent) {
    SM_GOTO(getState(STATE_ID_IDLE));
}

static void ReceiveData(Sale *parent) {
    receiveData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, receiveData, &parent->base.state, "ReceiveData");
    receiveData->vtable.enter = STATE_ENTER(ReceiveData);
    receiveData->vtable.exit = STATE_EXIT(ReceiveData);
    receiveData->vtable.handleKeypad = STATE_HANDLE(ReceiveData, KeypadEvent);
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

    EnterAmount(self);
    EnterPassword(self);
    Result(self);
    Connection(self);
    SendData(self);
    ReceiveData(self);
}