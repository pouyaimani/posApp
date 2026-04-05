#include "states.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "font/myFont.h"
#include "myColor.h"
#include "ui/ui.h"
#include "event.h"

static StMenu *menu;

STATE_DEF_ENTER(StMenu) {
    OOP_CALL(menu->menu, show);
}

STATE_DEF_EXIT(StMenu) {
    OOP_CALL(menu->menu, hide);
    uiDeleteMenu(menu->menu);
}

STATE_DEF_HANDLE(StMenu, TimeOutEvent) {

}

STATE_DEF_HANDLE(StMenu, KeypadEvent) {
    OOP_CALL(menu->menu, handleItem, ev->key);
    if (ev->key == KEY_ENTER) {
        if (menu->menu->checkEnable) {
            OOP_CALL(menu->menu, setChecked, menu->menu->idx);
        }
        if (menu->menu->togglable) {
            OOP_CALL(menu->menu, toggle, menu->menu->idx);
        }
        if (menu->menu->cb[menu->menu->idx]) {
            menu->menu->cb[menu->menu->idx](menu->menu->userData[menu->menu->idx]);
        }
        if (menu->menu->state[menu->menu->idx])
            SM_GOTO(menu->menu->state[menu->menu->idx]);
    } else if(ev->key <= KEY_9) {
        if (menu->menu->checkEnable) {
            OOP_CALL(menu->menu, setChecked, ev->key - 1);
        }
        if (menu->menu->togglable) {
            OOP_CALL(menu->menu, toggle, menu->menu->idx);
        }
        if (menu->menu->cb[ev->key - 1]) {
            menu->menu->cb[ev->key - 1](menu->menu->userData[menu->menu->idx]);
        }
        if (ev->key < menu->menu->cnt) {
            if (menu->menu->state[ev->key - 1])
                SM_GOTO(menu->menu->state[ev->key - 1]);
        }
    } else if (ev->key == KEY_ESC) {
        if(menu->onExit) {
            menu->onExit(menu->userData);
        }
        SM_GOTO(state->prev);
    }
}

OOP_CTOR(StMenu, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(StMenu);
    self->base.vtable.exit = STATE_EXIT(StMenu);
    self->base.vtable.handleKeypad = STATE_HANDLE(StMenu, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(StMenu, TimeOutEvent);

    menu = (StMenu*)getState(STATE_ID_MENU);
}