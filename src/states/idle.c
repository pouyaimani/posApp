#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "dev.h"
#include "mylvgl.h"
#include "display.h"
#include "event.h"
#include "wifi/wifi.h"
#include "assets.h"
#include "font/myFont.h"
#include "statusBar/statusBar.h"
#include "storage/storage.h"
#include "network/network.h"
#include "timer.h"

#define MENU_BAR_HEIGHT 46

static lv_obj_t *menuBar;
static lv_obj_t *swipCardText;
static lv_obj_t *swipCardCont;
static lv_obj_t *mainIcon;
static lv_obj_t *menuButton;
static lv_obj_t *menuIcon;
static lv_obj_t *menuText;

static Storage *storage;
static Wifi *wifi;

static Timer *timer;

static void wifiAutoConnect() {
    if (OOP_CALL(getNetwork(), getRoute) != NET_ROUTE_WIFI) {
        return;
    }
    WifiConnectSt_t conSt = OOP_CALL(getWifi(), getConnectStatus);
    if(conSt == WIFI_CONNECT_SUCCEED  || conSt == WIFI_CONNECT_UNDER_PROCESS) {
        return;
    }
    WifiApInfo_t apInfo = {0};
    char pwd[64] = {0};
    snprintf(apInfo.essid, sizeof(apInfo.essid), "%s", storage->settings->terminal.mWifiSSID);
    snprintf(apInfo.mac, sizeof(apInfo.mac), "%s", storage->settings->terminal.mWifiMac);
    apInfo.secMode = storage->settings->terminal.mWifiEnc;
    snprintf(pwd, sizeof(pwd), "%s", storage->settings->terminal.mWifiPwd);
    if (strlen(apInfo.essid) > 0 && strlen(pwd) > 0 && apInfo.secMode != 0) {
        OOP_CALL(wifi, hconnect, &apInfo, pwd);
    }
}

static void timerCb() {
    wifiAutoConnect();
}

static void menuEventCb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        SM_GOTO(getState(STATE_ID_SUPPORTER));
    }
}

STATE_DEF_ENTER(Idle) {
    CardHolder *ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = false;
    getEventloop()->registerChecker(getMagReader()->ioRead);
    LV_SHOW(menuBar);
    // LV_SHOW(swipCardCont);
    LV_SHOW(mainIcon);
    statusBar()->setInfoMode(STBAR_INFO_DATE);
}

STATE_DEF_EXIT(Idle) {
    getEventloop()->unregisterChecker(getMagReader()->ioRead);
    LV_HIDE(menuBar);
    // LV_HIDE(swipCardCont);
    LV_HIDE(mainIcon);
}

STATE_DEF_HANDLE(Idle, TimeOutEvent) {

}

STATE_DEF_HANDLE(Idle, KeypadEvent) {
    if (ev->key == KEY_FUNCTION) {
        SM_GOTO(getState(STATE_ID_SUPPORTER));
    }
}

STATE_DEF_HANDLE(Idle, MagEvent) {
    CardHolder *ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = true;
    SM_GOTO(getState(STATE_ID_CARD_HOLDER));
    LOG_TRACE("track1 = %s", ev->data->track1.data);
    LOG_TRACE("track2 = %s", ev->data->track2.data);
    LOG_TRACE("track3 = %s", ev->data->track3.data);
}

static void createUi() {
    menuBar = lv_obj_create(getDisplay()->screen);
    LV_SET_SIZE(menuBar, DISP_HOR_RES + 20, MENU_BAR_HEIGHT + 20);
    LV_SET_BG_COLOR(menuBar, MAIN_THEME_COLOR);
    LV_ALIGN(menuBar, LV_ALIGN_BOTTOM_MID, 10, 20);
    LV_SET_RADIUS(menuBar, 20);
    LV_SCROLL_DISABLE(menuBar);
    lv_obj_set_scroll_dir(menuBar, LV_DIR_NONE);
    LV_CLICK_DISABLE(menuBar);

    // swipCardCont = lv_obj_create(getDisplay()->screen);
    // LV_SET_SIZE(swipCardText, lv_pct(100), lv_pct(100));
    // LV_SET_BG_OPA(swipCardCont, LV_OPA_0);
    // LV_SET_BORDER_OPA(swipCardCont, LV_OPA_0);
    // LV_SCROLL_DISABLE(swipCardCont);
    // lv_obj_update_layout(swipCardCont);

    // swipCardText = lv_label_create(swipCardCont);
    // LV_SET_SIZE(swipCardText, 180, 20);
    // LV_SET_TEXT_COLOR(swipCardText, MAIN_THEME_COLOR);
    // LV_SET_TEXT_FONT(swipCardText, FONT_16);
    // LV_SET_TEXT(swipCardText, "لطفا کارت خود را بکشید");
    // LV_SCROLL_DISABLE(swipCardText);
    // LV_ALIGN(swipCardText, LV_ALIGN_RIGHT_MID, 70, -35);

    // lv_obj_update_layout(swipCardText);

    // lv_obj_set_style_transform_angle(swipCardText, -900, 0);
    // lv_obj_set_style_transform_pivot_x(swipCardText, 90, 0);
    // lv_obj_set_style_transform_pivot_y(swipCardText, 10 , 0);

    mainIcon = lv_img_create(getDisplay()->screen);
    lv_img_set_src(mainIcon, ICON_IDLE_MAIN);
    LV_ALIGN(mainIcon, LV_ALIGN_CENTER, 20, -30);
    LV_SCROLL_DISABLE(mainIcon);
    LV_CLICK_DISABLE(mainIcon);

    menuButton = lv_button_create(menuBar);
    LV_CLICKABLE(menuButton);
    LV_SET_BG_OPA(menuButton, LV_OPA_0);
    LV_SET_BORDER_OPA(menuButton, LV_OPA_0);
    LV_SET_SIZE(menuButton, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(menuButton, LV_ALIGN_TOP_RIGHT, -40, 5);
    LV_ADD_EV_CB(menuButton, menuEventCb, LV_EVENT_CLICKED, NULL);
    LV_SCROLL_DISABLE(menuButton);

    menuIcon = lv_img_create(menuButton);
    lv_img_set_src(menuIcon, ICON_MENU);
    LV_ALIGN(menuIcon, LV_ALIGN_TOP_MID, 0, 0);

    menuText = lv_label_create(menuButton);
    LV_SET_TEXT_FONT(menuText, FONT_16);
    LV_SET_TEXT_COLOR(menuText, COLOR_WHITE);
    LV_SET_TEXT(menuText, "منو");
    LV_ALIGN_TO(menuText, menuIcon, LV_ALIGN_OUT_BOTTOM_MID, 0, -5);
}

OOP_CTOR(Idle, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Idle);
    self->base.vtable.exit = STATE_EXIT(Idle);
    self->base.vtable.handleKeypad = STATE_HANDLE(Idle, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Idle, TimeOutEvent);
    self->base.vtable.handleMag = STATE_HANDLE(Idle, MagEvent);

    createUi();
    storage = getStorage();
    wifi = getWifi();

    timer = TIMER_CREATE(timerCb, SECS(10), false);
}