#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "event.h"
#include "assets.h"
#include "ui/ui.h"
#include "eventloop.h"
#include "storage/storage.h"
#include "network/network.h"
#include "iso8583.h"
#include "magReader/magReader.h"
#include "transmitter.h"
#include "phrases/phrases.h"

static Network *net;

static SubState *connecting;
static SubState *receiveData;
static SubState *sendData;

static int socketId;

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Connecting) {
    // connect();
    if (socketId < 0) {
        GOTO_INFO(STATE_IDLE, STATE_IDLE, phraseGetDef(PHRASE_CONNECTION_ERR), "");
        return;
    }
    SHOW_INFO(phraseGetDef(PHRASE_CONNECTIING), "");
}

STATE_DEF_EXIT(Connecting) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(Connecting, KeypadEvent) {
    SM_GOTO(sendData);
}

STATE_DEF_HANDLE(Connecting, SocketConnectEvent) {
    if(ev->isConnected) {
        GOTO_INFO(STATE_IDLE, STATE_IDLE, phraseGetDef(PHRASE_CONNECTION_ERR), "");
    } else {
        SM_GOTO(sendData);
    }
}

static void Connecting(State *parent) {
    connecting = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, connecting, parent, "Connection");
    connecting->vtable.enter = STATE_ENTER(Connecting);
    connecting->vtable.exit = STATE_EXIT(Connecting);
    connecting->vtable.handleKeypad = STATE_HANDLE(Connecting, KeypadEvent);
    connecting->vtable.onSocketConnect = STATE_HANDLE(Connecting, SocketConnectEvent);
}

/******************************************************************/

/******************** Send data sub state **********************/

static void prepareMsg() {
    char pan[32] = {0};
    getMagReader()->getPan(pan, sizeof(pan));
    // OOP_CALL(packer(), setPan, pan);
    // OOP_CALL(packer(), setTrack2, "");
    // OOP_CALL(packer(), setDateTime, "");
    // OOP_CALL(packer(), setTerminalId, "");
    // OOP_CALL(packer(), setRRN, "");
    // OOP_CALL(packer(), setMac, "", 0);
}

STATE_DEF_ENTER(SendData) {
    SHOW_INFO(phraseGetDef(PHRASE_SENDING_DATA), "");
}

STATE_DEF_EXIT(SendData) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(SendData, KeypadEvent) {
    SM_GOTO(receiveData);
}

STATE_DEF_HANDLE(SendData, SocketSentEvent) {
    SM_GOTO(receiveData);
}

static void SendData(State *parent) {
    sendData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, sendData, parent, "Send Data");
    sendData->vtable.enter = STATE_ENTER(SendData);
    sendData->vtable.exit = STATE_EXIT(SendData);
    sendData->vtable.handleKeypad = STATE_HANDLE(SendData, KeypadEvent);
    sendData->vtable.onSocketSent = STATE_HANDLE(SendData, SocketSentEvent);
}

/******************************************************************/

/******************** Receive data sub state **********************/

STATE_DEF_ENTER(ReceiveData) {
    SHOW_INFO(phraseGetDef(PHRASE_RECEIVING_DATA), "");
}

STATE_DEF_EXIT(ReceiveData) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(ReceiveData, KeypadEvent) {
    GOTO_IDLE();
}

STATE_DEF_HANDLE(ReceiveData, SocketReadyReadEvent) {
    GOTO_IDLE();
}

static void ReceiveData(State *parent) {
    receiveData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, receiveData, parent, "ReceiveData");
    receiveData->vtable.enter = STATE_ENTER(ReceiveData);
    receiveData->vtable.exit = STATE_EXIT(ReceiveData);
    receiveData->vtable.handleKeypad = STATE_HANDLE(ReceiveData, KeypadEvent);
    receiveData->vtable.onSocketReadyRead = STATE_HANDLE(ReceiveData, SocketReadyReadEvent);
}

/******************************************************************/

STATE_DEF_ENTER(IsoTransmitter) {
    SM_GOTO(connecting);
}

OOP_CTOR(IsoTransmitter, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(IsoTransmitter);

    Connecting(self);
    SendData(self);
    ReceiveData(self);

    net = network();
}