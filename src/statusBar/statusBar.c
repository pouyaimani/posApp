#include "statusBar.h"
#include "dev/dev.h"
#include "timer.h"
#include "logger.h"
#include "display.h"
#include "myLvgl.h"
#include "utility/utility.h"
#include "font/myFont.h"

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
    LV_SET_TEXT(dateBox, dt);
    memset(dt, 0, sizeof(dt));
    formatTimeStr(dt, sizeof(dt));
    LV_SET_TEXT(timeBox, dt);
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
    LV_SET_SIZE(dateBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(dateBox, LV_ALIGN_CENTER, 0, 0);
    LV_SET_BG_OPA(dateBox, LV_OPA_0);
    LV_SET_BORDER_OPA(dateBox, LV_OPA_0);
    LV_SET_TEXT_FONT(dateBox, FONT_16);
    LV_SET_TEXT_COLOR(dateBox, COLOR_WHITE);
    LV_SET_PAD_TOP(dateBox, 15);

    timeBox = lv_label_create(getDisplay()->statusbar);
    LV_SET_SIZE(timeBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(timeBox, LV_ALIGN_RIGHT_MID, -20, 0);
    LV_SET_BG_OPA(timeBox, LV_OPA_0);
    LV_SET_BORDER_OPA(timeBox, LV_OPA_0);
    LV_SET_TEXT_FONT(timeBox, FONT_16);
    LV_SET_TEXT_COLOR(timeBox, COLOR_WHITE);
    LV_SET_PAD_TOP(timeBox, 15);

    updateDateTime();
}

void createStatusBar() {
    CALL_ONCE(
        __statusBar = (StatusBar*)GET_MEM(sizeof(StatusBar));
        OOP_CALL_CTOR(StatusBar, __statusBar);
    );
    return __statusBar;
}