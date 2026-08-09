#include "states.h"
#include "event.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "font/myFont.h"
#include "myColor.h"
#include "ui/button.h"
#include "phrases/phrases.h"

static lv_obj_t* title;
static lv_obj_t* body;

static char*  textTitle;
static char*  textBody;
static Button confirmBut;
static Button cancelBut;

STATE_DEF_ENTER(Dialogue) {
    LV_SHOW(title);
    LV_SHOW(body);
    LV_SHOW(ui_button_obj(&confirmBut));
    LV_SHOW(ui_button_obj(&cancelBut));
}

STATE_DEF_EXIT(Dialogue) {
    LV_HIDE(title);
    LV_HIDE(body);
    LV_HIDE(ui_button_obj(&confirmBut));
    LV_HIDE(ui_button_obj(&cancelBut));
}

STATE_DEF_HANDLE(Dialogue, TimeOutEvent) {}

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
    LV_SET_TEXT_COLOR(title, COLOR_TEXT_PRIMARY);
    LV_SET_TEXT_ALIGN(title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(title, LV_ALIGN_CENTER, 0, -50);

    body = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(body, FONT_16);
    LV_SET_TEXT_COLOR(body, COLOR_TEXT_SECONDARY);
    LV_SET_TEXT_ALIGN(body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(body, LV_ALIGN_CENTER, 0, 0);

    ui_button_create(&confirmBut, disp()->screen);
    ui_button_set_text(&confirmBut, phraseGetDef(PHRASE_CONFIRM));
    ui_button_set_icon(&confirmBut, LV_SYMBOL_OK);
    ui_button_set_color(&confirmBut, lv_palette_main(LV_PALETTE_GREEN));
    LV_ALIGN(ui_button_obj(&confirmBut), LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    ui_button_create(&cancelBut, disp()->screen);
    ui_button_set_text(&cancelBut, phraseGetDef(PHRASE_CANCEL));
    ui_button_set_icon(&cancelBut, LV_SYMBOL_CLOSE);
    ui_button_set_color(&cancelBut, lv_palette_main(LV_PALETTE_RED));
    LV_ALIGN(ui_button_obj(&cancelBut), LV_ALIGN_BOTTOM_LEFT, 5, -10);

    LV_SET_TEXT(title, "");
    LV_SET_TEXT(body, "");
}

static void setTitle(const char* str) { LV_SET_TEXT(title, str); }

static void setBody(const char* str) { LV_SET_TEXT(body, str); }

OOP_CTOR(Dialogue, State* parent, const char* name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(Dialogue);
    self->base.vtable.exit          = STATE_EXIT(Dialogue);
    self->base.vtable.handleKeypad  = STATE_HANDLE(Dialogue, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Dialogue, TimeOutEvent);
    self->setTitle                  = setTitle;
    self->setBody                   = setBody;
    createUi();
}