#include "states.h"
#include "logger.h"
#include "dev.h"
#include "lvgl.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "services/services.h"
#include "ui/ui.h"

static lv_obj_t *items[SERVICE_ID_ALL];
static lv_obj_t *menu;

STATE_DEF_ENTER(Idle) {

}

STATE_DEF_EXIT(Idle) {

}

STATE_DEF_HANDLE(TimeOutEvent) {

}

STATE_DEF_HANDLE(KeypadEvent) {
    if (ev->key <= KEY_9) {
        ServiceId_t id = (ServiceId_t)((int)ev->key - 1);
        SM_GOTO(&getService(id)->state);
    }
}

static void createUi() {
    menu = uiMenu(getDisplay()->screen);
    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        items[i] = uiMenuAddItem(menu, getService(i)->state.name, NULL, NULL);
    }

}

OOP_CTOR(Idle, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Idle);
    self->base.vtable.exit = STATE_EXIT(Idle);
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);

    createUi();
}