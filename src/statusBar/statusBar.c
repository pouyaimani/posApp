#include "statusBar.h"
#include "dev/dev.h"
#include "timer.h"
#include "logger.h"
#include "display.h"
#include "myLvgl.h"
#include "utility/utility.h"
#include "font/myFont.h"
#include "wifi/wifi.h"
#include "cellular/cellular.h"
#include "assets.h"
#include "network/network.h"
#include "storage/storage.h"

static StatusBar *__statusBar;
static Timer *timer;
static StatusBarInfoMode_t infoMode = STBAR_INFO_DATE;
lv_obj_t *statusbar;
lv_obj_t *infoBox;
lv_obj_t *timeBox;
lv_obj_t *connectionIcon;
lv_obj_t *soundIcon;
lv_obj_t *batteryIcon;

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

static void updateBatteryIcon() {
    BatteryStat *bat = OOP_CALL(getDevice(), getBatteryStatus);
    if (bat->isChanrging) {
        lv_img_set_src(batteryIcon, ICON_BAT_CHARGING);
    } else {
        switch (bat->level) {
        case DEV_BAT_LEV_LOW:
            lv_img_set_src(batteryIcon, ICON_BAT_LEV_LOW);
            break;
        case DEV_BAT_LEV_1:
            lv_img_set_src(batteryIcon, ICON_BAT_LEV_1);
            break;
        case DEV_BAT_LEV_2:
            lv_img_set_src(batteryIcon, ICON_BAT_LEV_2);
            break;
        case DEV_BAT_LEV_3:
            lv_img_set_src(batteryIcon, ICON_BAT_LEV_3);
            break;
        default:
            break;
        }
    }
}

static void updateConnectionIcon() {
    SocketRoute_t route = OOP_CALL(getNetwork(), getRoute);
    if(1) {
        if (OOP_CALL(getWifi(), hgetConnectStatus) == WIFI_CONNECT_SUCCEED) {
            switch (OOP_CALL(getWifi(), getSignalStrength)) {
            case WIFI_SIGNAL_STRENGTH_0:
                lv_img_set_src(connectionIcon, ICON_WIFI_STRENGTH_0);
                break;
            case WIFI_SIGNAL_STRENGTH_1:
                lv_img_set_src(connectionIcon, ICON_WIFI_STRENGTH_1);
                break;
            case WIFI_SIGNAL_STRENGTH_2:
                lv_img_set_src(connectionIcon, ICON_WIFI_STRENGTH_2);
                break;
            case WIFI_SIGNAL_STRENGTH_3:
                lv_img_set_src(connectionIcon, ICON_WIFI_STRENGTH_3);
                break;
            default:
                break;
            }
        } else {
            lv_img_set_src(connectionIcon, ICON_WIFI_DISCONNECT);
        }
    } else if (route == NET_ROUTE_CELLUALR) {
        if (OOP_CALL(getCell(), getSignalStrength) == WIFI_CONNECT_SUCCEED) {
            switch (OOP_CALL(getCell(), getSignalStrength)) {
            case CELL_SIGNAL_STRENGTH_0:
                lv_img_set_src(connectionIcon, ICON_CELL_STRENGTH_0);
                break;
            case CELL_SIGNAL_STRENGTH_1:
                lv_img_set_src(connectionIcon, ICON_CELL_STRENGTH_1);
                break;
            case CELL_SIGNAL_STRENGTH_2:
                lv_img_set_src(connectionIcon, ICON_CELL_STRENGTH_2);
                break;
            case CELL_SIGNAL_STRENGTH_3:
                lv_img_set_src(connectionIcon, ICON_CELL_STRENGTH_3);
                break;
            default:
                break;
            }
        } else {
            lv_img_set_src(connectionIcon, ICON_CELL_DISCONNECT);
        }
    }
}

static void update() {
    if (infoMode == STBAR_INFO_DATE) {
        updateDate();
    }
    updateTime();
    updateBatteryIcon();
    updateConnectionIcon();

    if (getStorage()->settings->terminal.mVideoVolume > 0) {
        lv_img_set_src(soundIcon, ICON_SOUND_ON);
    } else {
        lv_img_set_src(soundIcon, ICON_SOUND_OFF);
    }
}

static void setInfoMode(StatusBarInfoMode_t mode) {
    infoMode = mode;
    update();
}

static void setInfo(const char *data) {
    LV_SET_TEXT(infoBox, data);
}

static void setSoundVolume(int volume) {
    switch (volume) {
    case 0:
        lv_img_set_src(soundIcon, ICON_SOUND_VOLUME_0);
        break;
    case 1:
        lv_img_set_src(soundIcon, ICON_SOUND_VOLUME_1);
        break;
    case 2:
        lv_img_set_src(soundIcon, ICON_SOUND_VOLUME_2);
        break;
    case 3:
        lv_img_set_src(soundIcon, ICON_SOUND_VOLUME_3);
        break;
    default:
        break;
    }
}

OOP_CTOR(StatusBar) {
    timer = TIMER_CREATE(update, SECS(2), false);

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
    LV_ALIGN(timeBox, LV_ALIGN_RIGHT_MID, -50, 0);
    LV_SET_BG_OPA(timeBox, LV_OPA_0);
    LV_SET_BORDER_OPA(timeBox, LV_OPA_0);
    LV_SET_TEXT_FONT(timeBox, FONT_16);
    LV_SET_TEXT_COLOR(timeBox, COLOR_WHITE);
    LV_SET_PAD_TOP(timeBox, 15);

    batteryIcon = lv_img_create(getDisplay()->statusbar);
    LV_ALIGN(batteryIcon, LV_ALIGN_RIGHT_MID, -15, 8);

    soundIcon = lv_img_create(getDisplay()->statusbar);
    LV_ALIGN(soundIcon, LV_ALIGN_LEFT_MID, 5, 8);

    connectionIcon = lv_img_create(getDisplay()->statusbar);
    LV_ALIGN(connectionIcon, LV_ALIGN_LEFT_MID, 40, 8);

    __statusBar->setInfo = setInfo;
    __statusBar->setInfoMode = setInfoMode;
    __statusBar->setSoundVolume = setSoundVolume;
    update();
}

StatusBar *statusBar() {
    CALL_ONCE(
        __statusBar = (StatusBar*)GET_MEM(sizeof(StatusBar));
        OOP_CALL_CTOR(StatusBar, __statusBar);
    );
    return __statusBar;
}