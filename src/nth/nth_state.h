#ifndef NTH_STATE_H_
#define NTH_STATE_H_

#include "oop.h"
#include "state.h"
#include "byteArray.h"

typedef struct NthTransaction NthTransaction;

OOP_CLASS(NthState) {
    OOP_EXTENDS(State);
    NthTransaction *tx;
    State *success;
    State *failure;
};

OOP_CTOR(
        NthState,
        NthTransaction *tx);

void GOTO_NTH(NthState *state);

#endif