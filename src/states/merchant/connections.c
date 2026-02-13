#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "ui/ui.h"
#include "dev/dev.h"

/******************** Merchant menu sub state **********************/

typedef enum {
    CONNECTION_WIFI = 0,
    CONNECTION_GPRS,
    CONNECTION_DIAL,
    CONNECTION_ALL
} ConnectionTypes_t;

ConnectionTypes_t menuMap[3];
int menuCount = 0;

static const char* itemTxt[CONNECTION_ALL] = {
    "وایفای",
    "gprs",
    "dial up",
};

static Menu menu;

static void createUi() {
    menu = uiMenu(getDisplay()->screen);
    if (getDevice()->module.wifi) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_WIFI], NULL, NULL);
        menuMap[menuCount++] = CONNECTION_WIFI;
    }
    if (getDevice()->module.gprs) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_GPRS], NULL, NULL);
        menuMap[menuCount++] = CONNECTION_GPRS;
    }
    if (getDevice()->module.dialup) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_DIAL], NULL, NULL);
        menuMap[menuCount++] = CONNECTION_DIAL;
    }       
}

static void destroyUi() {
    uiDeleteMenu(&menu);
}

STATE_DEF_ENTER(Connectios) {
    createUi();
    OOP_CALL(&menu, show);
}

STATE_DEF_EXIT(Connectios) {
    OOP_CALL(&menu, hide);
    destroyUi();
}

static void handleKeyAction(State *state, int id) {
    if (id >= menuCount) {
        return;
    }
    switch (menuMap[id]) {
    case CONNECTION_WIFI:
        break;
    default:
        break;
    }
}

STATE_DEF_HANDLE(Connectios, KeypadEvent) {
    OOP_CALL(&menu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        handleKeyAction(state, menu.idx);
    }  else {
        if (ev->key <= KEY_9) {
            int id = ((int)ev->key - 1);
            handleKeyAction(state, id);
        }
    }
}

OOP_CTOR(Connections, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, "merchant menu");
    self->base.vtable.enter = STATE_ENTER(Connectios);
    self->base.vtable.exit = STATE_EXIT(Connectios);
    self->base.vtable.handleKeypad = STATE_HANDLE(Connectios, KeypadEvent);
}