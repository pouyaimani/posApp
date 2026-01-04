#include "core/stateMachine/state.h"

typedef enum StateId_t {
    STATE_ID_START_UP,
    STATE_ID_IDLE
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