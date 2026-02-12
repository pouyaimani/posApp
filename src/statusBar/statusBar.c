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
static StatusBarInfoMode_t infoMode = STBAR_INFO_DATE;
lv_obj_t *statusbar;
lv_obj_t *infoBox;
lv_obj_t *timeBox;
lv_obj_t *wifiIcon;
lv_obj_t *soundIcon;

static void updateDate() {
    char dt[40];
    memset(dt, 0, sizeof(dt));
    formatDateTimeStr(dt, sizeof(dt));
    LV_SET_TEXT(infoBox, dt);
}

static void updateTime() {
    char dt[40];
    memset(dt, 0, sizeof(dt));
    formatTimeStr(dt, sizeof(dt));
    LV_SET_TEXT(timeBox, dt);
}

static void update() {
    BatteryStat *bat = OOP_CALL(getDevice(), getBatteryStatus);
    if (infoMode == STBAR_INFO_DATE) {
        updateDate();
    }
    updateTime();
    // LOG_TRACE("battery percent %d", bat->percent);
    // if (bat->isChanrging) {
    //     LOG_TRACE("battery is chargine ...");
    // }
}

static void setInfoMode(StatusBarInfoMode_t mode) {
    infoMode = mode;
    update();
}

static void setInfo(const char *data) {
    LV_SET_TEXT(infoBox, data);
}

OOP_CTOR(StatusBar) {
    timer = TIMER_CREATE(update, SECS(10), false);

    infoBox = lv_label_create(getDisplay()->statusbar);
    LV_SET_SIZE(infoBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(infoBox, LV_ALIGN_CENTER, 0, 0);
    LV_SET_BG_OPA(infoBox, LV_OPA_0);
    LV_SET_BORDER_OPA(infoBox, LV_OPA_0);
    LV_SET_TEXT_FONT(infoBox, FONT_16);
    LV_SET_TEXT_COLOR(infoBox, COLOR_WHITE);
    LV_SET_PAD_TOP(infoBox, 15);

    timeBox = lv_label_create(getDisplay()->statusbar);
    LV_SET_SIZE(timeBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(timeBox, LV_ALIGN_RIGHT_MID, -20, 0);
    LV_SET_BG_OPA(timeBox, LV_OPA_0);
    LV_SET_BORDER_OPA(timeBox, LV_OPA_0);
    LV_SET_TEXT_FONT(timeBox, FONT_16);
    LV_SET_TEXT_COLOR(timeBox, COLOR_WHITE);
    LV_SET_PAD_TOP(timeBox, 15);

    __statusBar->setInfo = setInfo;
    __statusBar->setInfoMode = setInfoMode;
    updateDate();
    updateTime();
}

StatusBar *statusBar() {
    CALL_ONCE(
        __statusBar = (StatusBar*)GET_MEM(sizeof(StatusBar));
        OOP_CALL_CTOR(StatusBar, __statusBar);
    );
    return __statusBar;
}