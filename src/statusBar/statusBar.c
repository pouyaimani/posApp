#include "statusBar.h"
#include "sys/sys.h"
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
#include "settings/settings.h"

static StatusBar *__statusBar;
static Timer *timer;
static StatusBarInfoMode_t infoMode = STBAR_INFO_DATE_TIME;
lv_obj_t *statusbar;
lv_obj_t *ldate;
lv_obj_t *ltime;
lv_obj_t *infoBox;
lv_obj_t *info;
lv_obj_t *connectionIcon;
lv_obj_t *soundIcon;
lv_obj_t *batteryIcon;

static void anim_y_cb(void * var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, v);
}

void dtScroll(lv_obj_t * label1, lv_obj_t * label2)
{
    lv_anim_t a1, a2;
    int32_t height = LV_GET_HEIGHT(getDisplay()->statusbar);

    lv_anim_init(&a1);
    lv_anim_set_var(&a1, label1);
    lv_anim_set_exec_cb(&a1, anim_y_cb);
    lv_anim_set_values(&a1, 0, -height);
    lv_anim_set_time(&a1, 300);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_in_out);
    lv_anim_start(&a1);

    lv_anim_init(&a2);
    lv_anim_set_var(&a2, label2);
    lv_anim_set_exec_cb(&a2, anim_y_cb);
    lv_anim_set_values(&a2, height, 0);
    lv_anim_set_time(&a2, 300);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_in_out);
    lv_anim_start(&a2);
}

static void updateDate() {
    char dt[40];
    memset(dt, 0, sizeof(dt));
    formatDateTimeStr(dt, sizeof(dt));
    LV_SET_TEXT(ldate, dt);
}

static void updateTime() {
    char dt[40];
    memset(dt, 0, sizeof(dt));
    formatTimeStr(dt, sizeof(dt));
    LV_SET_TEXT(ltime, dt);
}

static void updateBatteryIcon() {
    BatteryStat *bat = OOP_CALL(sys(), getBatteryStatus);
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
    NetRoute_t route = OOP_CALL(network(), getRoute);
    if(1) {
        if (OOP_CALL(getWifi(), getConnectStatus) == WIFI_CONNECT_SUCCEED) {
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
        if (OOP_CALL(getCell(), getSimStatus) != CELL_ERR_OK) {
            lv_img_set_src(connectionIcon, ICON_CELL_DISCONNECT);
            return;
        }
        if (OOP_CALL(getCell(), getPPPstatus) != CELL_PPP_SUCESS) {
            lv_img_set_src(connectionIcon, ICON_CELL_DISCONNECT);
            return;
        }
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
    }
}

static void showDateTime() {
    static uint8_t cnt = 0;
    cnt++;
    if (cnt == 5) {
        dtScroll(ldate, ltime);
    } else if (cnt == 10) {
        dtScroll(ltime, ldate);
        cnt = 0;
    }
}

static void update() {
    updateDate();
    updateTime();
    updateBatteryIcon();
    updateConnectionIcon();

    if (settings()->terminal.devVolume > 0) {
        lv_img_set_src(soundIcon, ICON_SOUND_ON);
    } else {
        lv_img_set_src(soundIcon, ICON_SOUND_OFF);
    }
    if (infoMode == STBAR_INFO_DATE_TIME) {
        showDateTime();
    }
}

static void enDateTimeMode() {
    infoMode = STBAR_INFO_DATE_TIME;
    LV_SHOW(ldate);
    LV_SHOW(ltime);
    LV_HIDE(info);
    update();
}

static void setInfo(const char *data) {
    infoMode = STBAR_INFO;
    LV_SET_TEXT(info, data);
    LV_HIDE(ldate);
    LV_HIDE(ltime);
    LV_SHOW(info);
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
    lv_obj_update_layout(getDisplay()->statusbar);

    infoBox = lv_obj_create(getDisplay()->statusbar);
    LV_SET_SIZE(infoBox, lv_pct(60), lv_pct(90));
    LV_ALIGN(infoBox, LV_ALIGN_CENTER, 0, 10);
    LV_SET_BG_OPA(infoBox, LV_OPA_0);
    LV_SET_BORDER_OPA(infoBox, LV_OPA_0);
    LV_SCROLL_DISABLE(infoBox);
    LV_SET_PAD_ALL(infoBox, 0);
    lv_obj_set_style_clip_corner(infoBox, true, 0);

    int32_t height = LV_GET_HEIGHT(getDisplay()->statusbar);
    ltime = lv_label_create(infoBox);
    LV_SET_SIZE(ltime, lv_pct(100), lv_pct(100));
    LV_ALIGN(ltime, LV_ALIGN_TOP_MID, 0, 0);
    LV_SET_BG_OPA(ltime, LV_OPA_0);
    LV_SET_BORDER_OPA(ltime, LV_OPA_0);
    LV_SET_TEXT_FONT(ltime, FONT_16);
    LV_SET_TEXT_COLOR(ltime, COLOR_WHITE);
    LV_SET_TEXT_ALIGN(ltime, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_y(ltime, height);

    ldate = lv_label_create(infoBox);
    LV_SET_SIZE(ldate, lv_pct(100), lv_pct(100));
    LV_ALIGN(ldate, LV_ALIGN_TOP_MID, 0, 0);
    LV_SET_BG_OPA(ldate, LV_OPA_0);
    LV_SET_BORDER_OPA(ldate, LV_OPA_0);
    LV_SET_TEXT_FONT(ldate, FONT_16);
    LV_SET_TEXT_COLOR(ldate, COLOR_WHITE);
    LV_SET_TEXT_ALIGN(ldate, LV_TEXT_ALIGN_CENTER);

    info = lv_label_create(infoBox);
    LV_SET_SIZE(info, lv_pct(100), lv_pct(100));
    LV_ALIGN(info, LV_ALIGN_TOP_MID, 0, 0);
    LV_SET_BG_OPA(info, LV_OPA_0);
    LV_SET_BORDER_OPA(info, LV_OPA_0);
    LV_SET_TEXT_FONT(info, FONT_16);
    LV_SET_TEXT_COLOR(info, COLOR_WHITE);
    LV_SET_TEXT_ALIGN(info, LV_TEXT_ALIGN_CENTER);

    batteryIcon = lv_img_create(getDisplay()->statusbar);
    LV_ALIGN(batteryIcon, LV_ALIGN_RIGHT_MID, -15, 5);

    soundIcon = lv_img_create(getDisplay()->statusbar);
    LV_ALIGN(soundIcon, LV_ALIGN_LEFT_MID, 5, 5);

    connectionIcon = lv_img_create(getDisplay()->statusbar);
    LV_ALIGN(connectionIcon, LV_ALIGN_LEFT_MID, 40, 5);

    __statusBar->setInfo = setInfo;
    __statusBar->enDateTimeMode = enDateTimeMode;
    __statusBar->setSoundVolume = setSoundVolume;
    enDateTimeMode();
}

StatusBar *statusBar() {
    CALL_ONCE(
        __statusBar = (StatusBar*)GET_MEM(sizeof(StatusBar));
        OOP_CALL_CTOR(StatusBar, __statusBar);
    );
    return __statusBar;
}