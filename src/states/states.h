#include "core/stateMachine/state.h"

typedef enum StateId_t {
    STATE_ID_START_UP,
    STATE_ID_INFO,
    STATE_ID_IDLE,
    STATE_ID_INPUT,
    STATE_ID_CARD_HOLDER,
    STATE_ID_DIALOGUE
} StateId_t;

State *getState(StateId_t id);


/*********************Startup*********************/

OOP_CLASS(Startup)
{
    OOP_EXTENDS(State);
};

OOP_CTOR(Startup, State *parent, const char *name);

/**********************Idle**********************/

OOP_CLASS(Idle)
{
    OOP_EXTENDS(State);
};

OOP_CTOR(Idle, State *parent, const char *name);

/**********************Input**********************/

typedef enum {
    IN_MODE_AMOUNT,
    IN_MODE_PASSWORD,
    IN_MODE_NUMBERS
} InputMode_t;

OOP_CLASS(Input)
{
    OOP_EXTENDS(State);
    OOP_METHOD(void, setMode, InputMode_t);
    OOP_METHOD(void, setTitle, const char *);
    OOP_METHOD(void, setMax, int);
};

OOP_CTOR(Input, State *parent, const char *name);

/**********************Card Holder**********************/

OOP_CLASS(CardHolder)
{
    OOP_EXTENDS(State);
};

OOP_CTOR(CardHolder, State *parent, const char *name);

/**************************Info*************************/

OOP_CLASS(Info)
{
    OOP_EXTENDS(State);
    OOP_METHOD(void, setBody, const char *);
    OOP_METHOD(void, setTitle, const char *);
};

OOP_CTOR(Info, State *parent, const char *name);

#define SHOW_INFO(state, title, body)                           \
    OOP_CALL(getState(STATE_ID_INFO), setNext, state);          \
    Info *info = (Info *)getState(STATE_ID_INFO);               \
    info->setTitle(title);                                      \
    info->setBody(body);                                        \
    SM_GOTO(getState(STATE_ID_INFO));                           

/************************Dialogue***********************/

OOP_CLASS(Dialogue)
{
    OOP_EXTENDS(State);
    OOP_METHOD(void, setBody, const char *);
    OOP_METHOD(void, setTitle, const char *);
};

OOP_CTOR(Dialogue, State *parent, const char *name);

#define SHOW_DIAL(state, title, body)                           \
    OOP_CALL(getState(STATE_ID_DIALOGUE), setNext, state);      \
    Info *info = (Info *)getState(STATE_ID_DIALOGUE);           \
    info->setTitle(title);                                      \
    info->setBody(body);                                        \
    SM_GOTO(getState(STATE_ID_DIALOGUE));        