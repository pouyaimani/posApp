#include "states.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "utility/utility.h"
#include "font/myFont.h"
#include "myColor.h"

static lv_obj_t *title;
static lv_obj_t *body;

static char *textTitle;
static char *textBody;

STATE_DEF_ENTER(Info) {
    LV_SHOW(title);
    LV_SHOW(body);
}

STATE_DEF_EXIT(Info) {
    LV_HIDE(title);
    LV_HIDE(body);
}

STATE_DEF_HANDLE(TimeOutEvent) {

}

STATE_DEF_HANDLE(KeypadEvent) {
    SM_GOTO(state->next); 
}

static void createUi() {
    title = lv_label_create(getDisplay()->screen);
    LV_SET_TEXT_FONT(title, FONT_20);
    LV_SET_TEXT_COLOR(title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(title, LV_ALIGN_CENTER, 0, -50);
    
    body = lv_label_create(getDisplay()->screen);
    LV_SET_TEXT_FONT(body, FONT_16);
    LV_SET_TEXT_COLOR(body, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(body, LV_ALIGN_CENTER, 0, 0);

    LV_SET_TEXT(title, "");
    LV_SET_TEXT(body, "");
}

static void setTitle(const char *str) {
    LV_SET_TEXT(title, str);
}

static void setBody(const char *str) {
    LV_SET_TEXT(body, str);
}

OOP_CTOR(Info, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Info);
    self->base.vtable.exit = STATE_EXIT(Info);
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->setTitle = setTitle;
    self->setBody = setBody;
    createUi();
}