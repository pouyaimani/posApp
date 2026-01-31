#include "statusBar.h"
#include "dev/dev.h"
#include "timer.h"
#include "logger.h"
#include "display.h"
#include "lvgl.h"
#include "utility/utility.h"

static StatusBar *__statusBar;
static Timer *timer;
lv_obj_t *statusbar;
lv_obj_t *dateBox;
lv_obj_t *timeBox;
lv_obj_t *wifiIcon;
lv_obj_t *soundIcon;

static void updateDateTime() {
    char dt[40];
    memset(dt, 0, sizeof(dt));
    formatDateTimeStr(dt, sizeof(dt));
    lv_label_set_text(dateBox, dt);
    memset(dt, 0, sizeof(dt));
    formatTimeStr(dt, sizeof(dt));
    lv_label_set_text(timeBox, dt);;
}

static void update() {
    BatteryStat *bat = OOP_CALL(getDevice(), getBatteryStatus);
    updateDateTime();
    // LOG_TRACE("battery percent %d", bat->percent);
    // if (bat->isChanrging) {
    //     LOG_TRACE("battery is chargine ...");
    // }
}

OOP_CTOR(StatusBar) {
    timer = TIMER_CREATE(update, SECS(10), false);
    dateBox = lv_label_create(getDisplay()->statusbar);
    lv_obj_set_size(dateBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(dateBox, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(dateBox, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(dateBox, LV_OPA_0, 0);
    lv_obj_set_style_text_font(dateBox, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(dateBox, lv_color_white(), 0);
    lv_obj_set_style_pad_top(dateBox, 15, 0);

    timeBox = lv_label_create(getDisplay()->statusbar);
    lv_obj_set_size(timeBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(timeBox, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_opa(timeBox, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(timeBox, LV_OPA_0, 0);
    lv_obj_set_style_text_font(timeBox, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(timeBox, lv_color_white(), 0);
    lv_obj_set_style_pad_top(timeBox, 15, 0);

    updateDateTime();
}

void createStatusBar() {
    CALL_ONCE(
        __statusBar = (StatusBar*)GET_MEM(sizeof(StatusBar));
        OOP_CALL_CTOR(StatusBar, __statusBar);
    );
    return __statusBar;
}