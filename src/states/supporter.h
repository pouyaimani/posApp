#ifndef SUPPORTER_H_
#define SUPPORTER_H_

#include "state.h"
#include "merchant/merchant.h"
#include "supervisor/supervisor.h"

/**********************************************************************
 *
 *                          Supporter States
 *
 **********************************************************************/

OOP_CLASS(SupporterSubStates) {
    SubState*   customer;
    Merchant*   merchant;
    Supervisor* supervisor;
    SubState*   powerOff;
};

OOP_CLASS(Supporter) {
    OOP_EXTENDS(State);
    SupporterSubStates substate;
};

OOP_CTOR(Supporter, State* parent, const char* name);

#endif