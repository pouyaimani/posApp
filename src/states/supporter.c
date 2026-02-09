#include "states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "services/services.h"
#include "ui/ui.h"
#include "dev/dev.h"

static Menu menu;
static SubState *powerOff;

#define ITEM_CNT_MAX    4

static const char* itemTxt[ITEM_CNT_MAX] = {
    "مشتری",
    "پذیرنده",
    "سرپرست",
    "خاموش کردن",
};

STATE_DEF_ENTER(Supporter) {
    OOP_CALL(&menu, show);
}

STATE_DEF_EXIT(Supporter) {
    OOP_CALL(&menu, hide);
}

STATE_DEF_HANDLE(Supporter, TimeOutEvent) {

}

static void handleKeyAction(State *state, int id) {
    if (id >= ITEM_CNT_MAX) {
        return;
    }
    if (id == 3) {
        SHOW_DIAL(state, powerOff, "قصد خروج دارید؟", "");
    }
}

STATE_DEF_HANDLE(Supporter, KeypadEvent) {
    OOP_CALL(&menu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(getState(STATE_ID_IDLE));
    } else if (ev->key == KEY_ENTER) {
        int id = OOP_CALL(&menu, getIdx);
        handleKeyAction(state, id);
    }  else {
    if (ev->key <= KEY_9) {
        int id = ((int)ev->key - 1);
        handleKeyAction(state, id);
    }
    }
}

static void createUi() {
    menu = uiMenu(getDisplay()->screen);
    for (uint8_t i = 0; i < ITEM_CNT_MAX ; i++) {
        OOP_CALL(&menu, addItem, itemTxt[i], NULL, NULL);
    }
}

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(PowerOff) {
    OOP_CALL(getDevice(), powerOff);
}

STATE_DEF_EXIT(PowerOff) {

}

static void PowerOff(Sale *parent) {
    powerOff = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, powerOff, &parent->base.state, "power off");
    powerOff->vtable.enter = STATE_ENTER(PowerOff);
    powerOff->vtable.exit = STATE_EXIT(PowerOff);
}

OOP_CTOR(Supporter, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Supporter);
    self->base.vtable.exit = STATE_EXIT(Supporter);
    self->base.vtable.handleKeypad = STATE_HANDLE(Supporter, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Supporter, TimeOutEvent);

    createUi();
    PowerOff(self);
}