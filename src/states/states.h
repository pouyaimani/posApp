
#ifndef STATES_H_
#define STATES_H_

#include "state.h"
#include <stdbool.h>
#include "common.h"
#include "byteArray.h"
#include "txn.h"
#include "txnflow/txnFLow.h"

typedef enum StateId_t {
    STATE_ID_START_UP,
    STATE_ID_INFO,
    STATE_ID_GINFO,
    STATE_ID_IDLE,
    STATE_ID_INPUT,
    STATE_ID_CARD_HOLDER,
    STATE_ID_DIALOGUE,
    STATE_ID_SUPPORTER,
    STATE_ID_DEV_INFO,
    STATE_ID_FIXED_AMOUNT,
    STATE_ID_MENU,
    STATE_ID_ISO_TRANSMITTER,
    STATE_ID_HTTP_TRANSMITTER,
    STATE_ID_NET_CONNECT,
    STATE_ID_NET_SEND,
    STATE_ID_NET_RECEIVE

} StateId_t;

State* getState(StateId_t id);

/*********************Startup*********************/

OOP_CLASS(Startup) { OOP_EXTENDS(State); };

OOP_CTOR(Startup, State* parent, const char* name);

/**********************Idle**********************/

OOP_CLASS(Idle) { OOP_EXTENDS(State); };

OOP_CTOR(Idle, State* parent, const char* name);

#define STATE_IDLE  getState(STATE_ID_IDLE)
#define GOTO_IDLE() SM_GOTO(STATE_IDLE)

/**********************Card Holder**********************/

OOP_CLASS(CardHolder) {
    OOP_EXTENDS(State);
    bool isMagSwiped;
};

OOP_CTOR(CardHolder, State* parent, const char* name);

/**************************Info*************************/
OOP_CLASS(Info) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setData, int, const char*, const char*);
};

OOP_CTOR(Info, State* parent, const char* name);

#define STATE_INFO getState(STATE_ID_INFO)

/************************Dev Info*************************/

OOP_CLASS(DevInfo) { OOP_EXTENDS(State); };

OOP_CTOR(DevInfo, State* parent, const char* name);

/************************Dialogue***********************/

OOP_CLASS(Dialogue) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setBody, const char*);
    OOP_METHOD(void, setTitle, const char*);
    CallBack_t cb;
    void*      cbData;
};

OOP_CTOR(Dialogue, State* parent, const char* name);

void GOTO_DIAL(State* prev, State* next, const char* title, const char* body,
               CallBack_t cb, void* data);

/************************ Fixed Amount ***********************/

OOP_CLASS(FixedAmount) { OOP_EXTENDS(State); };

OOP_CTOR(FixedAmount, State* parent, const char* name);

// Helper functions

#define STATE_SUPPORTER  getState(STATE_ID_SUPPORTER)
#define GOTO_SUPPORTER() SM_GOTO(STATE_SUPPORTER)

#endif
