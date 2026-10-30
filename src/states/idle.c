#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "sys/sys.h"
#include "mylvgl.h"
#include "display.h"
#include "eventAll.h"
#include "wifi/wifi.h"
#include "cellular/cellular.h"
#include "assets.h"
#include "font/myFont.h"
#include "statusBar/statusBar.h"
#include "storage/storage.h"
#include "network/network.h"
#include "timer.h"
#include "states/merchant/merchant.h"
#include "settings/settings.h"
#include "record/txnRecs.h"
#include "record/shiftRecs.h"
#include "utility/utility.h"
#include "receipt/receiptTemplates.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "file/file.h"
#include "ui/swipeHint.h"
#include "ui/digitalReceipt.h"
#include "assets.h"
#include "txnMgr.h"
#include "infoMgr/infoMgr.h"

#define MENU_BAR_HEIGHT 46

static lv_obj_t* menuBar;
static lv_obj_t* swipCardText;
static lv_obj_t* swipCardCont;
static lv_obj_t* mainIcon;
static lv_obj_t* menuButton;
static lv_obj_t* menuIcon;
static lv_obj_t* menuText;
static lv_obj_t* merchantName;

static Timer*    timer;
static SwipeHint swipe;

static void showDevInfo(void* arg) {
    State* state = (State*)arg;
    infoMgrsetCommon(state, state);
    infoMgr()->runDevInfo(&infoFinishCtx);
}

static void go2Supporter(void* arg) {
    VAR_UNUSED(arg);
    SM_GOTO(getState(STATE_ID_SUPPORTER));
}

void showKahrobaMenu(void* arg);

static const KeyHandler keyHandler[] = {
    {KEY_0, showKahrobaMenu},
    {KEY_1, NULL},
    {KEY_2, NULL},
    {KEY_3, NULL},
    {KEY_4, NULL},
    {KEY_5, NULL},
    {KEY_6, NULL},
    {KEY_7, NULL},
    {KEY_8, NULL},
    {KEY_9, NULL},
    {KEY_ENTER, NULL},
    {KEY_CLEAR, showDevInfo},
    {KEY_ESC, NULL},
    {KEY_FUNCTION, go2Supporter},
};

static void wifiAutoConnect() {
    WifiScanSt_t    scanSt    = OOP_CALL(wifi(), hgetScanStatus);
    WifiConnectSt_t connectSt = OOP_CALL(wifi(), getConnectStatus);
    LOG_TRACE("Wifi auto connect: scan status = %d, connect status = %d",
              scanSt, connectSt);
    if (connectSt == WIFI_CONNECT_FAILED && scanSt == WIFI_SCAN_FAILED &&
        !wifi()->onUserSelection) {
        wifi()->startScan();
    }
}

STATE_DEF_HANDLE(Idle, WifiEvent) {
    LOG_TRACE("ev->type = %d", ev->type);
    if (ev->type == WIFI_EV_SCAN) {
        if (ev->scanStatus == WIFI_SCAN_SUCCEED) {
            LOG_TRACE("saved wifi ssid = %s", settings()->terminal.wfiSSID);
            LOG_TRACE("wifi()->apList.size = %d", wifi()->apList.size);
            for (uint8_t i = 0; i < wifi()->apList.size; i++) {
                DEFINE_STRING(safeSsid, 64);
                normalizeSsid(wifi()->apList.list[i].essid,
                              sizeof(wifi()->apList.list[i].essid), safeSsid,
                              sizeof(safeSsid));
                if (strcmp(wifi()->apList.list[i].essid,
                           settings()->terminal.wfiSSID) == 0) {
                    WifiApInfo_t apInfo  = {0};
                    char         pwd[64] = {0};
                    snprintf(apInfo.essid, sizeof(apInfo.essid), "%s",
                             settings()->terminal.wfiSSID);
                    snprintf(apInfo.mac, sizeof(apInfo.mac), "%s",
                             settings()->terminal.wifiMac);
                    apInfo.secMode = settings()->terminal.wifiEnc;
                    snprintf(pwd, sizeof(pwd), "%s",
                             settings()->terminal.wifiPwd);
                    LOG_TRACE("Saved wifi ssid [%s%s%s] is found nearby.",
                              LOG_COLOR_RED, apInfo.essid, LOG_COLOR_RESET);
                    RETURN_IF_LE(strlen(apInfo.essid), 0, ;);
                    RETURN_IF_LE(strlen(pwd), 0, ;);
                    RETURN_IF(apInfo.secMode, 0, ;);
                    LOG_TRACE("start connecting to [%s%s%s]", LOG_COLOR_RED,
                              apInfo.essid, LOG_COLOR_RESET);
                    wifi()->connect(&apInfo, pwd);
                }
            }
        }
    } else if (ev->type == WIFI_EV_CONNECT) {
        LOG_TRACE("result of connecting to [%s%s%s] -> %d", LOG_COLOR_RED,
                  settings()->terminal.wfiSSID, LOG_COLOR_RESET,
                  ev->connectStatus);
    }
}

static void cellAutoConnect() {
    if (OOP_CALL(cellular(), getSimStatus) != SIM_STATUS_OK) {
        LOG_DEBUG("sim status error");
        return;
    }
    CellPPPStatus_t st = OOP_CALL(cellular(), getPPPstatus);
    // LOG_DEBUG("cellular status = %d", st);
    if (st == CELL_PPP_SUCESS || st == CELL_PPP_DIALING) {
        return;
    }
    cellular()->startPPPlogin(NULL, NULL, NULL, NULL);
}

static void netAutoConnect(void* arg) {
    VAR_UNUSED(arg);
    NetRoute_t route = OOP_CALL(network(), getRoute);
    if (route == NET_ROUTE_WIFI) {
        wifiAutoConnect();
    } else if (route == NET_ROUTE_CELLULAR) {
        cellAutoConnect();
    }
}

static void hibernate(void* arg) {
    Idle* idle = (Idle*)arg;
    OOP_CALL(sys(), hibernate);
    OOP_CALL(&idle->base, updateTimers);
}

static void powerOff(void* arg) {
    VAR_UNUSED(arg);
    OOP_CALL(sys(), powerOff);
}

static void setSleepeTimeOutIfChanged(State* idle) {
    static uint32_t sleepTimeout = 0;
    if (settings()->terminal.sleepTimeout != sleepTimeout) {
        sleepTimeout = settings()->terminal.sleepTimeout;
        OOP_CALL(idle, resetTimerTimeOut, hibernate, MINUTES(sleepTimeout));
    }
}

static void setPowerOffTimeOutIfChanged(State* idle) {
    static uint32_t powerOffTimeOut = 0;
    if (settings()->terminal.powerOffTimeOut != powerOffTimeOut) {
        powerOffTimeOut = settings()->terminal.powerOffTimeOut;
        OOP_CALL(idle, resetTimerTimeOut, powerOff, MINUTES(powerOffTimeOut));
    }
}

static void menuEventCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        SM_GOTO(getState(STATE_ID_SUPPORTER));
    }
}

static void setTextIfChanged(lv_obj_t* label, const char* txt) {
    if (strcmp(lv_label_get_text(label), txt) == 0) {
        return;
    }

    LV_SET_TEXT(label, txt);
}

STATE_DEF_ENTER(Idle) {
    setSleepeTimeOutIfChanged(state);
    setPowerOffTimeOutIfChanged(state);
    OOP_CALL(cellular(), checkSimStatus);
    netAutoConnect(NULL);
    CardHolder* ch  = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = false;
    getEventloop()->registerChecker(magreader()->ioRead);
    LOG_DEBUG("settings()->terminal.isCfgDone  = %d",
              settings()->terminal.isCfgDone);
    setTextIfChanged(merchantName,
                     settings()->terminal.isCfgDone
                         ? settings()->terminal.merchantName
                         : phraseGetDef(PHRASE_DEV_IS_NOT_CONFIGURED));
    LV_SHOW(menuBar);
    // LV_SHOW(swipCardCont);
    LV_SHOW(mainIcon);
    statusBar()->enDateTimeMode();
    shifts();
    swipeHintShow(&swipe);
}

STATE_DEF_EXIT(Idle) {
    getEventloop()->unregisterChecker(magreader()->ioRead);
    LV_HIDE(menuBar);
    // LV_HIDE(swipCardCont);
    LV_HIDE(mainIcon);
    swipeHintHide(&swipe);
}

STATE_DEF_HANDLE(Idle, TimeOutEvent) {}

STATE_DEF_HANDLE(Idle, KeypadEvent) {
    KeyHandler* handler =
        findKeyHandler(keyHandler, ARRAY_COUNT(keyHandler), ev->key);
    if (handler) {
        if (handler->handle)
            handler->handle(state);
    } else if (ev->key == KEY_1) {
    } else if (ev->key == KEY_2) {
    } else if (ev->key == KEY_3) {
        OOP_CALL(file(), remove, "/mtd0/txn_t_info");
    } else if (ev->key == KEY_4) {

    } else if (ev->key == KEY_5) {
        txnMgr()->pendingReset();
    } else if (ev->key == KEY_6) {
    } else if (ev->key == KEY_7) {
    } else if (ev->key == KEY_8) {
        txnrecord()->reset();
    } else if (ev->key == KEY_9) {
        txnrecord()->init();
    } else if (ev->key == KEY_0) {
    }
}

STATE_DEF_HANDLE(Idle, MagEvent) {
    if (!settings()->terminal.isCfgDone) {
        infoMgrsetCommon(STATE_IDLE, STATE_IDLE);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_DEV_IS_NOT_CONFIGURED), "");
        return;
    }
    CardHolder* ch  = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = true;
    if (settings()->terminal.fixedAmountItem == FIXED_AMNT_DIS) {
        SM_GOTO(getState(STATE_ID_CARD_HOLDER));
    } else {
        SM_GOTO(getState(STATE_ID_FIXED_AMOUNT));
    }
    LOG_TRACE("track1 = %s", ev->data->track1.data);
    LOG_TRACE("track2 = %s", ev->data->track2.data);
    LOG_TRACE("track3 = %s", ev->data->track3.data);
}

static void createUi() {
    menuBar = lv_obj_create(disp()->screen);
    LV_SET_SIZE(menuBar, DISP_HOR_RES + 20, MENU_BAR_HEIGHT + 20);
    LV_SET_BG_COLOR(menuBar, MAIN_THEME_COLOR);
    LV_ALIGN(menuBar, LV_ALIGN_BOTTOM_MID, 10, 20);
    LV_SET_RADIUS(menuBar, 20);
    LV_SCROLL_DISABLE(menuBar);
    lv_obj_set_scroll_dir(menuBar, LV_DIR_NONE);
    LV_CLICK_DISABLE(menuBar);

    merchantName = lv_label_create(menuBar);
    LV_SET_TEXT_FONT(merchantName, FONT_16);
    LV_SET_TEXT_COLOR(merchantName, COLOR_WHITE);
    LV_SET_TEXT(merchantName, settings()->terminal.isCfgDone
                                  ? settings()->terminal.merchantName
                                  : phraseGetDef(PHRASE_DEV_IS_NOT_CONFIGURED));
    LV_ALIGN(merchantName, LV_ALIGN_CENTER, -15, -10);

    mainIcon = lv_img_create(disp()->screen);
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
    LV_SET_TEXT(menuText, phraseGetDef(PHRASE_MENU));
    LV_ALIGN_TO(menuText, menuIcon, LV_ALIGN_OUT_BOTTOM_MID, 0, -5);

    swipeHintCreate(&swipe,
                    disp()->screen, // parent
                    SWIPE_DOWN,     // animation direction
                    7);             // number of arrows

    swipeHintAlign(&swipe, LV_ALIGN_RIGHT_MID, 20, -10);
}

OOP_CTOR(Idle, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(Idle);
    self->base.vtable.exit          = STATE_EXIT(Idle);
    self->base.vtable.handleKeypad  = STATE_HANDLE(Idle, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Idle, TimeOutEvent);
    self->base.vtable.handleMag     = STATE_HANDLE(Idle, MagEvent);
    self->base.vtable.handleWifi    = STATE_HANDLE(Idle, WifiEvent);

    createUi();

    OOP_CALL(&self->base, enableTimer);
    OOP_CALL(&self->base, addTimer, hibernate,
             MINUTES(settings()->terminal.sleepTimeout), self);
    OOP_CALL(&self->base, addTimer, powerOff,
             MINUTES(settings()->terminal.powerOffTimeOut), self);

    OOP_CALL(&self->base, addTimer, netAutoConnect, SECS(10), self);

    // timer = TIMER_CREATE(netAutoConnect, SECS(10), false);
    // TIMER_START(timer);
}