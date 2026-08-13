
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
    STATE_ID_TXN_RES,
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

/**********************Input**********************/

#define INPUT_MAX_LEN 64

typedef enum {
    IN_MODE_AMOUNT,
    IN_MODE_PASSWORD,
    IN_MODE_NUMBERS,
    IN_MODE_ALPHAB,
    IN_MODE_TIME,
    IN_MODE_DATE,
    IN_MODE_IP,
} InputMode_t;

OOP_CLASS(Input) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setMode, InputMode_t);
    OOP_METHOD(void, setData, const char*, const char*);
    OOP_METHOD(void, setMax, int);
    OOP_METHOD(void, reset);
    OOP_METHOD(void, setInput, const char*);
    char* input;
    char* password;
    char* ip;
    char* out;
};

OOP_CTOR(Input, State* parent, const char* name);

#define STATE_INPUT getState(STATE_ID_INPUT)

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

#define GOTO_DEV_INFO(prev)                                                    \
    DevInfo* devInfo = (DevInfo*)getState(STATE_ID_DEV_INFO);                  \
    OOP_CALL(getState(STATE_ID_DEV_INFO), setPrev, prev);                      \
    OOP_CALL(getState(STATE_ID_DEV_INFO), setNext, prev);                      \
    SM_GOTO(getState(STATE_ID_DEV_INFO));

/**************************Ginfo*************************/

OOP_CLASS(Ginfo) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setText, const char*);
    OOP_METHOD(void, setImg, const char*);
};

OOP_CTOR(Ginfo, State* parent, const char* name);

#define GOTO_GINFO(state, text, img)                                           \
    OOP_CALL(getState(STATE_ID_GINFO), setNext, state);                        \
    Ginfo* info = (Ginfo*)getState(STATE_ID_GINFO);                            \
    info->setText(title);                                                      \
    info->setImg(body);                                                        \
    SM_GOTO(getState(STATE_ID_GINFO));

/************************Dialogue***********************/

OOP_CLASS(Dialogue) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setBody, const char*);
    OOP_METHOD(void, setTitle, const char*);
};

OOP_CTOR(Dialogue, State* parent, const char* name);

#define GOTO_DIAL(prev, next, title, body)                                     \
    OOP_CALL(getState(STATE_ID_DIALOGUE), setNext, next);                      \
    OOP_CALL(getState(STATE_ID_DIALOGUE), setPrev, prev);                      \
    Dialogue* dial = (Dialogue*)getState(STATE_ID_DIALOGUE);                   \
    dial->setTitle(title);                                                     \
    dial->setBody(body);                                                       \
    SM_GOTO(getState(STATE_ID_DIALOGUE));

/************************ Menu ***********************/
typedef struct Menu Menu;

OOP_CLASS(StMenu) {
    OOP_EXTENDS(State);
    Menu*      menu;
    CallBack_t onExit;
    void*      userData;
};

OOP_CTOR(StMenu, State* parent, const char* name);

/************************ Fixed Amount ***********************/

OOP_CLASS(FixedAmount) { OOP_EXTENDS(State); };

OOP_CTOR(FixedAmount, State* parent, const char* name);

/*************************** Txn Result ***********************/

OOP_CLASS(TxnResult) {
    OOP_EXTENDS(State);
    TxnData        data;
    TxnFlowConfig* txnCfg;
    TxnFlowStatus* st;
    State*         onSuccess;
    State*         onFailure;
};

OOP_CTOR(TxnResult, State* parent, const char* name);

#define STATE_TXN_RES getState(STATE_ID_TXN_RES)

// Helper functions

#define STATE_SUPPORTER  getState(STATE_ID_SUPPORTER)
#define GOTO_SUPPORTER() SM_GOTO(STATE_SUPPORTER)

void GOTO_INPUT(State* prev, State* next, const char* title, const char* body,
                int max, InputMode_t mode, char* out);
void GOTO_INFO(State* prev, State* next, uint8_t type, const char* title,
               const char* body);
void GOTO_MENU(State* prev, Menu* amenu, CallBack_t _onExit, void* userData);
void GOTO_TXN_RES(TxnFlowConfig* cfg, TxnData* data, const TxnFlowStatus* st,
                  State* onSuccess, State* onFailure);
void GOTO_NET_CONNNECT();
void GOTO_NET_SEND();
void GOTO_NET_RECEIVE();

#endif