#include "core/stateMachine/state.h"

typedef enum StateId_t {
    STATE_ID_START_UP,
    STATE_ID_IDLE,
    STATE_ID_INPUT
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
} InputMode_t;

OOP_CLASS(Input)
{
    OOP_EXTENDS(State);
    OOP_METHOD(void, setPrevState, State *);
    OOP_METHOD(void, setNextState, State *);
    OOP_METHOD(void, setMode, InputMode_t);
};

OOP_CTOR(Input, State *parent, const char *name);