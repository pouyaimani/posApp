#include "core/stateMachine/state.h"
#include <stdbool.h>

OOP_CLASS(Merchant) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Merchant, State *parent, const char *name);

OOP_CLASS(Connections) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Connections, State *parent, const char *name);

OOP_CLASS(Settings) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Settings, State *parent, const char *name);

OOP_CLASS(Reports) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Reports, State *parent, const char *name);