#include "states.h"
#include "logger.h"
#include "dev.h"
#include "lvgl.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "services/services.h"
#include "ui/ui.h"

static Menu menu;

STATE_DEF_ENTER(CardHolder) {
    OOP_CALL(&menu, show);
}

STATE_DEF_EXIT(CardHolder) {
    OOP_CALL(&menu, hide);
}

STATE_DEF_HANDLE(CardHolder, TimeOutEvent) {

}

STATE_DEF_HANDLE(CardHolder, KeypadEvent) {
    OOP_CALL(&menu, handleItem, ev->key);
    if (ev->key <= KEY_9) {
        ServiceId_t id = (ServiceId_t)((int)ev->key - 1);
        SM_GOTO(&getService(id)->state);
    }  else if (ev->key == KEY_ESC) {
        SM_GOTO(getState(STATE_ID_IDLE));
    } else if (ev->key == KEY_ENTER) {
        ServiceId_t id = (ServiceId_t)menu.idx;
        SM_GOTO(&getService(id)->state);        
    }
}

static void createUi() {
    menu = uiMenu(getDisplay()->screen);
    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        OOP_CALL(&menu, addItem, getService(i)->state.name, NULL, NULL);
    }
}

OOP_CTOR(CardHolder, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(CardHolder);
    self->base.vtable.exit = STATE_EXIT(CardHolder);
    self->base.vtable.handleKeypad = STATE_HANDLE(CardHolder, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(CardHolder, TimeOutEvent);

    createUi();
}