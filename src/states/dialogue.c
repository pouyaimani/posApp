#include "states.h"
#include "event.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "font/myFont.h"
#include "myColor.h"
#include "ui/ui.h"

static lv_obj_t *title;
static lv_obj_t *body;

static char *textTitle;
static char *textBody;
static Button confirmBut;
static Button cancelBut;

STATE_DEF_ENTER(Dialogue) {
    LV_SHOW(title);
    LV_SHOW(body);
    LV_SHOW(confirmBut.main);
    LV_SHOW(cancelBut.main);
}

STATE_DEF_EXIT(Dialogue) {
    LV_HIDE(title);
    LV_HIDE(body);
    LV_HIDE(confirmBut.main);
    LV_HIDE(cancelBut.main);
}

STATE_DEF_HANDLE(Dialogue, TimeOutEvent) {

}

STATE_DEF_HANDLE(Dialogue, KeypadEvent) {
    if (ev->key == KEY_ENTER) {
        SM_GOTO(state->next);
    } else if (ev->key == KEY_ESC) {
        SM_GOTO(state->prev);
    } 
}

static void createUi() {
    title = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(title, FONT_20);
    LV_SET_TEXT_COLOR(title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(title, LV_ALIGN_CENTER, 0, -50);
    
    body = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(body, FONT_16);
    LV_SET_TEXT_COLOR(body, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(body, LV_ALIGN_CENTER, 0, 0);

    confirmBut = uiButton(disp()->screen, 0x68DD40, "تایید");
    LV_ALIGN(confirmBut.main, LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    cancelBut = uiButton(disp()->screen, 0xFF4E4E, "لغو");
    LV_ALIGN(cancelBut.main, LV_ALIGN_BOTTOM_LEFT, 5, -10);

    LV_SET_TEXT(title, "");
    LV_SET_TEXT(body, "");
}

static void setTitle(const char *str) {
    LV_SET_TEXT(title, str);
}

static void setBody(const char *str) {
    LV_SET_TEXT(body, str);
}

OOP_CTOR(Dialogue, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Dialogue);
    self->base.vtable.exit = STATE_EXIT(Dialogue);
    self->base.vtable.handleKeypad = STATE_HANDLE(Dialogue, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Dialogue, TimeOutEvent);
    self->setTitle = setTitle;
    self->setBody = setBody;
    createUi();
}