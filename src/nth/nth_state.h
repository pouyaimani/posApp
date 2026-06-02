#ifndef NTH_STATE_H_
#define NTH_STATE_H_

#include "oop.h"
#include "state.h"
#include "byteArray.h"

typedef struct NthTransaction NthTransaction;

OOP_CLASS(NthState) {
    OOP_EXTENDS(State);
    NthTransaction *tx;
    State *onSucess;
    State *onFailure;
    ByteArray *txData;
};

OOP_CTOR(NthState, State *parent, NthTransaction *tx, const char *name);

#endif