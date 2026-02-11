#include "services.h"
#include "states/states.h"
#include "dev/dev.h"
#include "ui/ui.h"

#define PASSWORD_MAX_LEN    4

static SubState *enterAmount;
static SubState *enterPass;
static SubState *connection;
static SubState *receiveData;
static SubState *sendData;
static SubState *result;

STATE_DEF_ENTER(Sale) {
    SM_GOTO(enterAmount);
}

STATE_DEF_EXIT(Sale) {

}

/******************** Enter amount sub state **********************/

#define AMOUNT_MAX_CNT  12

STATE_DEF_ENTER(EnterAmount) {
    Input * in = (Input*)getState(STATE_ID_INPUT);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, getState(STATE_ID_IDLE));
    OOP_CALL(getState(STATE_ID_INPUT), setNext, enterPass);
    in->reset();
    in->setMode(IN_MODE_AMOUNT);
    in->setTitle("مبلغ");
    in->setMax(AMOUNT_MAX_CNT);
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
    Input * in = (Input*)getState(STATE_ID_INPUT);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, getState(STATE_ID_IDLE));
    OOP_CALL(getState(STATE_ID_INPUT), setNext, connection);
    in->reset();
    in->setMode(IN_MODE_PASSWORD);
    in->setTitle("رمز کارت");
    in->setMax(PASSWORD_MAX_LEN);
    SM_GOTO(getState(STATE_ID_INPUT));
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

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Connection) {
    InfoPage info = infoPage();
    OOP_CALL(&info, show);
    OOP_CALL(&info, setData, INFO_T_TEXT, "در حال اتصال", "");
}

STATE_DEF_EXIT(Connection) {
    InfoPage info = infoPage();
    OOP_CALL(&info, hide);
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
    InfoPage info = infoPage();
    OOP_CALL(&info, setData, INFO_T_TEXT, "ارسال اطلاعات", "");
    OOP_CALL(&info, show);
}

STATE_DEF_EXIT(SendData) {
    InfoPage info = infoPage();
    OOP_CALL(&info, hide);
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
    InfoPage info = infoPage();
    OOP_CALL(&info, show);
    OOP_CALL(&info, setData, INFO_T_TEXT, "دریافت اطلاعات", "");
}

STATE_DEF_EXIT(ReceiveData) {
    InfoPage info = infoPage();
    OOP_CALL(&info, hide);
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
    self->base.state.vtable.exit = STATE_EXIT(Sale);

    EnterAmount(self);
    EnterPassword(self);
    Result(self);
    Connection(self);
    SendData(self);
    ReceiveData(self);
}