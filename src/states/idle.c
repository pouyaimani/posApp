#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "dev.h"
#include "lvgl.h"
#include "display.h"
#include "event.h"
#include "wifi/wifi.h"

#define MENU_BAR_HEIGHT 46

static lv_obj_t *menuBar;
static lv_obj_t *swipCardText;

STATE_DEF_ENTER() {
    getEventloop()->registerChecker(getMagReader()->ioRead);
    lv_obj_remove_flag(menuBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(swipCardText, LV_OBJ_FLAG_HIDDEN);
}

STATE_DEF_EXIT() {
    getEventloop()->unregisterChecker(getMagReader()->ioRead);
    lv_obj_add_flag(menuBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(swipCardText, LV_OBJ_FLAG_HIDDEN);
}

STATE_DEF_HANDLE(TimeOutEvent) {

}

STATE_DEF_HANDLE(KeypadEvent) {
    LOG_TRACE("event successfully is reached. key = %d", ev->key);
    if (ev->key == KEY_ESC) {
        OOP_CALL(getDevice(), powerOff);
    } else if (ev->key == KEY_1) {
        WIFI_START_SCAN();
    } else if (ev->key == KEY_2) {
        Input * in = (Input*) getState(STATE_ID_INPUT);
        in->setPrevState(getState(STATE_ID_IDLE));
        in->setNextState(getState(STATE_ID_IDLE));
        in->setMode(IN_MODE_AMOUNT);
        SM_GOTO(getState(STATE_ID_INPUT));
    }
}

STATE_DEF_HANDLE(MagEvent) {
    return;
    // LOG_TRACE("Mag event is recieved.");
    // LOG_TRACE("track1 = %s", ev->data->track1);
    // LOG_TRACE("track2 = %s", ev->data->track2);
    // LOG_TRACE("track3 = %s", ev->data->track3);
}

STATE_DEF_HANDLE(WifiEvent) {
    LOG_TRACE("WIFI event is received.");
    for (int i = 0 ; i < ev->apList->size ; i++) {
        LOG_TRACE("essid: %s", ev->apList->list[i].essid);
    }
}

static void createUi() {
    menuBar = lv_obj_create(getDisplay()->screen);
    lv_obj_set_size(menuBar, DISP_HOR_RES + 20, MENU_BAR_HEIGHT + 20);
    lv_obj_set_style_bg_color(menuBar, lv_color_hex(MAIN_THEME_COLOR), 0);
    lv_obj_align(menuBar, LV_ALIGN_BOTTOM_MID, 10, 20);
    lv_obj_set_style_radius(menuBar, 20, 0);

    swipCardText = lv_label_create(getDisplay()->screen);
    lv_obj_set_style_text_font(swipCardText, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_label_set_text(swipCardText, "لطفا کارت خود را بکشید");
    lv_obj_align(swipCardText, LV_ALIGN_CENTER, 0, 0);
}

OOP_CTOR(Idle, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER();
    self->base.vtable.exit = STATE_EXIT();
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->base.vtable.handleMag = STATE_HANDLE(MagEvent);
    self->base.vtable.handleWifi = STATE_HANDLE(WifiEvent);

    createUi();
}