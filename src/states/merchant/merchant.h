#ifndef MERCHANT_H_
#define MERCHANT_H_

#include "core/stateMachine/state.h"
#include <stdbool.h>

OOP_CLASS(Connections) { OOP_EXTENDS(State); };

OOP_CTOR(Connections, State* parent, const char* name);

OOP_CLASS(Settings) { OOP_EXTENDS(State); };

OOP_CTOR(Settings, State* parent, const char* name);

OOP_CLASS(Reports) { OOP_EXTENDS(State); };

OOP_CTOR(Reports, State* parent, const char* name);

OOP_CLASS(Shift) { OOP_EXTENDS(State); };

OOP_CTOR(Shift, State* parent, const char* name);

OOP_CLASS(OtherProjects) { OOP_EXTENDS(State); };

OOP_CTOR(OtherProjects, State* parent, const char* name);

typedef enum {
    FIXED_AMNT_DIS = 0,
    FIXED_AMNT_LISTS,
    FIXED_AMNT_SINGLE,
    FIXED_AMNT_VARIANT,
    FIXED_AMNT_ALL
} FixedAmntItem_t;

OOP_CLASS(MerchantData) { OOP_EXTENDS(State); };

OOP_CTOR(MerchantData, State* parent, const char* name);

OOP_CLASS(MerchantSubStates) {
    Connections*   connection;
    Settings*      settings;
    Reports*       reports;
    Shift*         shifts;
    OtherProjects* otherProjects;
};

OOP_CLASS(Merchant) {
    OOP_EXTENDS(State);
    MerchantSubStates subState;
};

OOP_CTOR(Merchant, State* parent, const char* name);

#endif