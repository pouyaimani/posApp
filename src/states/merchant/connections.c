#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/menu.h"
#include "sys/sys.h"
#include "wifi/wifi.h"
#include "storage/storage.h"
#include "network/network.h"
#include "cellular/cellular.h"
#include "settings/settings.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "input/inputMgr.h"

static Cellular* cel;
static Network*  net;
static SubState* wifiScan;
static SubState* wifiConnect;
static SubState* wifiEnterPass;

static SubState* cellularLogin;

WifiApInfo_t* selectedAp;

#define WIFI_DISCONNECT_STATE    0
#define WIFI_CONNECT_STATE       1
#define WIFI_AFTER_CONNECT_STATE 2

int connectState = WIFI_DISCONNECT_STATE;

/******************** Wifi connect sub state **********************/

STATE_DEF_ENTER(WifiConnect) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CONNECTING_2_WIFI),
              phraseGetDef(PHRASE_PLEASE_WAIT));
    connectState = WIFI_DISCONNECT_STATE;
    wifi()->disconnect();
}

STATE_DEF_EXIT(WifiConnect) {}

static void saveWifiInfo(State* parent, WifiApInfo_t* ap, const char* pwd) {
    snprintf(settings()->terminal.wfiSSID, sizeof(settings()->terminal.wfiSSID),
             "%s", ap->essid);
    snprintf(settings()->terminal.wifiMac, sizeof(settings()->terminal.wifiMac),
             "%s", ap->mac);
    settings()->terminal.wifiEnc = ap->secMode;
    snprintf(settings()->terminal.wifiPwd, sizeof(settings()->terminal.wifiPwd),
             "%s", pwd);
    LOG_TRACE("saveWifiInfo: setting route to %d", NET_ROUTE_WIFI);
    Result_t res = network()->setRoute(NET_ROUTE_WIFI);
    if (res.err != ERR_DSC_OK) {
        LOG_TRACE("Unable to set device route.");
        GOTO_INFO(parent, parent, INFO_ERROR,
                  phraseGetDef(PHRASE_CONNECTION_ERR), "");
        return;
    }
    // settings()->terminal.netRoute = NET_ROUTE_WIFI;
    settings()->save();
}

STATE_DEF_HANDLE(WifiConnect, WifiEvent) {
    if (connectState == WIFI_DISCONNECT_STATE) {
        wifi()->connect(selectedAp, inmgr()->input);
        connectState = WIFI_CONNECT_STATE;
    } else {
        LOG_TRACE("Wifi: connect status = %d", ev->connectStatus);
        if (ev->connectStatus == WIFI_CONNECT_SUCCEED) {
            GOTO_INFO(state->parent, state->parent, INFO_SUCCESS,
                      phraseGetDef(PHRASE_CONNECTION_SUCCEED), "");
            saveWifiInfo(state->parent, selectedAp, inmgr()->input);
        } else {
            GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                      phraseGetDef(PHRASE_CONNECTION_ERR), "");
        }
    }
}

static void WifiConnect(State* parent) {
    wifiConnect = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, wifiConnect, parent, "wifi connect");
    wifiConnect->vtable.enter      = STATE_ENTER(WifiConnect);
    wifiConnect->vtable.exit       = STATE_EXIT(WifiConnect);
    wifiConnect->vtable.handleWifi = STATE_HANDLE(WifiConnect, WifiEvent);
}

/******************** Wifi Enter pass sub state **********************/

STATE_DEF_ENTER(WifiEnterPass) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_ALPHAB,
            .title  = phraseGetDef(PHRASE_WIFI_PIN),
            .info   = "",
            .maxLen = 32,
        },
        state->parent, wifiConnect, NULL, NULL);
}

STATE_DEF_EXIT(WifiEnterPass) {}

static void WifiEnterPass(State* parent) {
    wifiEnterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, wifiEnterPass, parent, "wifi enter pass");
    wifiEnterPass->vtable.enter = STATE_ENTER(WifiEnterPass);
    wifiEnterPass->vtable.exit  = STATE_EXIT(WifiEnterPass);
}

/******************** Wifi scan sub state **********************/

static Menu* wifiMenu = NULL;

STATE_DEF_ENTER(WifiScan) {
    Result_t res                  = network()->init(NET_ROUTE_WIFI);
    settings()->terminal.netRoute = NET_ROUTE_WIFI;
    settings()->save();
    if (res.err != ERR_DSC_OK) {
        LOG_ERROR("Network init error = %d", res.err);
    }
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_SEARCHING_4_WIFI),
              phraseGetDef(PHRASE_PLEASE_WAIT));
    if (wifi()->startScan() != WIFI_ERR_OK) {
        GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                  phraseGetDef(PHRASE_SEARCHING_WIFI_ERR), "");
    }
}

STATE_DEF_EXIT(WifiScan) {
    if (wifiMenu) {
        ui_menu_destroy(wifiMenu);
        MEM_FREE(wifiMenu);
        wifiMenu = NULL;
    }
}

STATE_DEF_HANDLE(WifiScan, KeypadEvent) {
    if (wifi()->scanSt == WIFI_SCAN_UNDER_PROCESS) {
        return;
    } else if (wifi()->scanSt == WIFI_SCAN_FAILED) {
    } else {
        ui_menu_handleItem(wifiMenu, ev->key);
        if (ev->key == KEY_ESC) {
            ui_menu_hide(wifiMenu);
            SM_GOTO(state->parent);
        } else if (ev->key == KEY_ENTER) {
            ui_menu_hide(wifiMenu);
            SM_GOTO(wifiEnterPass);
            selectedAp = &wifi()->apList.list[wifiMenu->idx];
        }
    }
}

STATE_DEF_HANDLE(WifiScan, WifiEvent) {
    if (ev->scanStatus == WIFI_SCAN_SUCCEED) {
        wifiMenu = MEM_ALLOC(sizeof(*wifiMenu));
        ui_menu_create(wifiMenu, disp()->screen);
        LOG_TRACE("wifi()->apList.size = %d", wifi()->apList.size);
        for (uint8_t i = 0; i < wifi()->apList.size; i++) {
            DEFINE_STRING(safeSsid, 64);
            normalizeSsid(wifi()->apList.list[i].essid, safeSsid);
            ui_menu_addItem(wifiMenu, safeSsid, LV_TEXT_ALIGN_RIGHT,
                            wifiEnterPass, NULL, NULL);
        }
        HIDE_INFO();
        ui_menu_show(wifiMenu);
    } else if (ev->scanStatus == WIFI_SCAN_FAILED) {
        GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                  phraseGetDef(PHRASE_SEARCHING_WIFI_ERR), "");
    }
}

static void WifiScan(State* parent) {
    wifiScan = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, wifiScan, parent, "wifi Scan");
    wifiScan->vtable.enter        = STATE_ENTER(WifiScan);
    wifiScan->vtable.exit         = STATE_EXIT(WifiScan);
    wifiScan->vtable.handleWifi   = STATE_HANDLE(WifiScan, WifiEvent);
    wifiScan->vtable.handleKeypad = STATE_HANDLE(WifiScan, KeypadEvent);
}

/******************** Cellular connect sub state **********************/

STATE_DEF_ENTER(CellularLogin) {
    Result_t res                  = network()->init(NET_ROUTE_CELLULAR);
    settings()->terminal.netRoute = NET_ROUTE_CELLULAR;
    settings()->save();
    if (res.err != ERR_DSC_OK) {
        LOG_ERROR("Network init error = %d", res.err);
    }
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CONNECTIING_2_NET),
              phraseGetDef(PHRASE_PLEASE_WAIT));
    OOP_CALL(cel, checkSimStatus);
    if (OOP_CALL(cel, getSimStatus) != SIM_STATUS_OK) {
        GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                  phraseGetDef(PHRASE_CONNECTION_ERR),
                  phraseGetDef(PHRASE_CHECK_SIM_STAT));
        return;
    }
    cel->startPPPlogin(NULL, NULL, NULL, NULL);
}

STATE_DEF_EXIT(CellularLogin) {}

STATE_DEF_HANDLE(CellularLogin, CellEvent) {
    if (ev->pppSt == CELL_PPP_SUCESS) {
        // Result_t res = network()->setRoute(NET_ROUTE_CELLULAR);
        // if (res.err != ERR_DSC_OK) {
        //     LOG_TRACE("Unable to set device route.");
        //     GOTO_INFO(state->parent, state->parent, INFO_ERROR,
        //               phraseGetDef(PHRASE_CONNECTION_ERR), "");
        //     return;
        // }
        // settings()->terminal.netRoute = NET_ROUTE_CELLULAR;
        // settings()->save();
        GOTO_INFO(state->parent, state->parent, INFO_SUCCESS,
                  phraseGetDef(PHRASE_CONNECTION_SUCCEED), "");
    } else if (ev->pppSt == CELL_PPP_FAILURE) {
        GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                  phraseGetDef(PHRASE_CONNECTION_ERR), "");
    } else if (ev->pppSt == CELL_PPP_INVALID) {
        GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                  phraseGetDef(PHRASE_CONNECTION_ERR), "");
    }
}

STATE_DEF_HANDLE(CellularLogin, KeypadEvent) {}

static void CellularLogin(State* parent) {
    cellularLogin = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, cellularLogin, parent, "cellular login");
    cellularLogin->vtable.enter = STATE_ENTER(CellularLogin);
    cellularLogin->vtable.exit  = STATE_EXIT(CellularLogin);
    cellularLogin->vtable.handleKeypad =
        STATE_HANDLE(CellularLogin, KeypadEvent);
    cellularLogin->vtable.handleCell = STATE_HANDLE(CellularLogin, CellEvent);
}

/******************** Connection sub state **********************/

typedef enum {
    CONNECTION_WIFI = 0,
    CONNECTION_GPRS,
    CONNECTION_DIAL,
    CONNECTION_ALL
} ConnectionTypes_t;

ConnectionTypes_t menuMap[3];
int               menuCount = 0;

static Menu* menu;

static void createUi() {
    ui_menu_create(menu, disp()->screen);
    menuCount        = 0;
    NetRoute_t route = OOP_CALL(net, getRoute);
    if (sys()->module.wifi) {
        ui_menu_addItem(menu, phraseGetDef(PHRASE_WIFI), LV_TEXT_ALIGN_RIGHT,
                        wifiScan, NULL, NULL);
        menuMap[menuCount] = CONNECTION_WIFI;
        if (route == NET_ROUTE_WIFI) {
            ui_menu_set_checked(menu, menuCount);
        }
        menuCount++;
    }
    if (sys()->module.gprs) {
        ui_menu_addItem(menu, phraseGetDef(PHRASE_GPRS), LV_TEXT_ALIGN_RIGHT,
                        cellularLogin, NULL, NULL);
        menuMap[menuCount] = CONNECTION_GPRS;
        if (route == NET_ROUTE_CELLULAR) {
            ui_menu_set_checked(menu, menuCount);
        }
        menuCount++;
    }
}

STATE_DEF_ENTER(Connectios) {
    createUi();
    GOTO_MENU(state->parent, menu, NULL, NULL);
}

OOP_CTOR(Connections, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, "connections");
    self->base.vtable.enter = STATE_ENTER(Connectios);

    WifiScan(self);
    WifiConnect(self);
    WifiEnterPass(self);
    CellularLogin(self);

    cel = cellular();
    net = network();

    menu = MEM_ALLOC(sizeof(*menu));
}