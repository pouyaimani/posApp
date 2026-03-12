#include "states.h"
#include "logger.h"
#include "dev.h"
#include "lvgl.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "services/services.h"
#include "ui/ui.h"
#include "eventloop.h"

static SubState *connecting;
static SubState *receiveData;
static SubState *sendData;

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Connecting) {
    SHOW_INFO("در حال اتصال", "");
}

STATE_DEF_EXIT(Connecting) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(Connecting, KeypadEvent) {
    SM_GOTO(sendData);
}

static void Connecting(State *parent) {
    connecting = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, connecting, parent, "Connection");
    connecting->vtable.enter = STATE_ENTER(Connecting);
    connecting->vtable.exit = STATE_EXIT(Connecting);
    connecting->vtable.handleKeypad = STATE_HANDLE(Connecting, KeypadEvent);
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

static void SendData(State *parent) {
    sendData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, sendData, parent, "Send Data");
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
    GOTO_IDLE();
}

static void ReceiveData(State *parent) {
    receiveData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, receiveData, parent, "ReceiveData");
    receiveData->vtable.enter = STATE_ENTER(ReceiveData);
    receiveData->vtable.exit = STATE_EXIT(ReceiveData);
    receiveData->vtable.handleKeypad = STATE_HANDLE(ReceiveData, KeypadEvent);
}

/******************************************************************/

STATE_DEF_ENTER(Communication) {
    SM_GOTO(connecting);
}

OOP_CTOR(Communication, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Communication);

    Connecting(self);
    SendData(self);
    ReceiveData(self);
}