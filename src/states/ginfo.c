#include "states.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "utility/utility.h"
#include "font/myFont.h"
#include "myColor.h"

static lv_obj_t *img;
static lv_obj_t *line;
static lv_obj_t *text;

STATE_DEF_ENTER(Ginfo) {
    LV_SHOW(img);
    LV_SHOW(line);
    LV_SHOW(text);
}

STATE_DEF_EXIT(Ginfo) {
    LV_HIDE(img);
    LV_HIDE(line);
    LV_HIDE(text);
}

STATE_DEF_HANDLE(Ginfo, TimeOutEvent) {

}

STATE_DEF_HANDLE(Ginfo, KeypadEvent) {
    SM_GOTO(state->next); 
}

static void createUi() {
    text = lv_label_create(getDisplay()->screen);
    LV_SET_TEXT_FONT(text, FONT_20);
    LV_SET_TEXT_COLOR(text, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(text, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(text, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(text, LV_ALIGN_TOP_MID, 0, -161);
    
    img = lv_img_create(getDisplay()->screen);
    LV_ALIGN(img, LV_ALIGN_TOP_MID, 0, -36);
    LV_SCROLL_DISABLE(img);
    LV_CLICK_DISABLE(img);

    line = lv_obj_create(getDisplay()->screen);
    LV_SET_SIZE(line, 190, 8);
    LV_ALIGN(line, LV_ALIGN_TOP_MID, 0, -140);
    LV_SET_RADIUS(line, 17);
    LV_SET_BG_COLOR(line, 0x333333);
    LV_SET_BORDER_COLOR(line, 0x333333);
}

static void setText(const char *str) {
    LV_SET_TEXT(text, str);
}

static void setImg(const char *src) {
    lv_img_set_src(img, src);
}

OOP_CTOR(Ginfo, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Ginfo);
    self->base.vtable.exit = STATE_EXIT(Ginfo);
    self->base.vtable.handleKeypad = STATE_HANDLE(Ginfo, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Ginfo, TimeOutEvent);
    self->setText= setText;
    self->setImg = setImg;
    createUi();
}