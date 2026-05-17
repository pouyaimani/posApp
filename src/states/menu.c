#include "states.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "font/myFont.h"
#include "myColor.h"
#include "ui/menu.h"
#include "event.h"

static StMenu *menu;

STATE_DEF_ENTER(StMenu) {
    ui_menu_show(menu->menu);
}

STATE_DEF_EXIT(StMenu) {
    ui_menu_hide(menu->menu);
    ui_menu_destroy(menu->menu);
}

STATE_DEF_HANDLE(StMenu, TimeOutEvent) {

}

STATE_DEF_HANDLE(StMenu, KeypadEvent) {
    ui_menu_handleItem(menu->menu, ev->key);
    if (ev->key == KEY_ENTER) {
        if (menu->menu->checkEnable) {
            ui_menu_set_checked(menu->menu, menu->menu->idx);
        }
        if (menu->menu->togglable) {
            ui_menu_toggle(menu->menu, menu->menu->idx);
        }
        if (menu->menu->cb[menu->menu->idx]) {
            menu->menu->cb[menu->menu->idx](menu->menu->userData[menu->menu->idx]);
        }
        if (menu->menu->state[menu->menu->idx])
            SM_GOTO(menu->menu->state[menu->menu->idx]);
    } else if(ev->key <= KEY_9) {
        if (menu->menu->checkEnable) {
            ui_menu_set_checked(menu->menu, ev->key - 1);
        }
        if (menu->menu->togglable) {
            ui_menu_toggle(menu->menu, menu->menu->idx);
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