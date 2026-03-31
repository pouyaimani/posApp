#include "state.h"
#include <stdbool.h>

OOP_CLASS(Supervisor) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Supervisor, State *parent, const char *name);

OOP_CLASS(NetworkSettings) {
    OOP_EXTENDS(State);
};

OOP_CTOR(NetworkSettings, State *parent, const char *name);

OOP_CLASS(KeyInjection) {
    OOP_EXTENDS(State);
};

OOP_CTOR(KeyInjection, State *parent, const char *name);

OOP_CLASS(Configuration) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Configuration, State *parent, const char *name);

OOP_CLASS(FARA) {
    OOP_EXTENDS(State);
};

OOP_CTOR(FARA, State *parent, const char *name);

OOP_CLASS(MerchantPassReset) {
    OOP_EXTENDS(State);
};

OOP_CTOR(MerchantPassReset, State *parent, const char *name);

OOP_CLASS(UpdateApp) {
    OOP_EXTENDS(State);
};

OOP_CTOR(UpdateApp, State *parent, const char *name);

OOP_CLASS(DefaultSettings) {
    OOP_EXTENDS(State);
};

OOP_CTOR(DefaultSettings, State *parent, const char *name);