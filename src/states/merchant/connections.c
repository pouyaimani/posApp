#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "wifi/wifi.h"
#include "storage/storage.h"
#include "network/network.h"
#include "cellular/cellular.h"

static Wifi *wifi;
static Cellular *cel;
static Network *net;
static Storage *storage;
static SubState *wifiScan;
static SubState *wifiConnect;
static SubState *wifiEnterPass;

static SubState *cellularLogin;

WifiApInfo_t *selectedAp;

#define WIFI_DISCONNECT_STATE       0
#define WIFI_CONNECT_STATE          1
#define WIFI_AFTER_CONNECT_STATE    2

int connectState = WIFI_DISCONNECT_STATE;

/******************** Wifi connect sub state **********************/

STATE_DEF_ENTER(WifiConnect) {
    SHOW_INFO("wifi در حال اتصال به", "لطفا منتظر بمانید");
    connectState = WIFI_DISCONNECT_STATE;
    wifi->disconnect();
}

STATE_DEF_EXIT(WifiConnect) {
    
}

static void saveWifiInfo(WifiApInfo_t *ap, const char *pwd) {
    snprintf(storage->settings->terminal.wfiSSID, 
        sizeof(storage->settings->terminal.wfiSSID), "%s", ap->essid);
    snprintf(storage->settings->terminal.wifiMac,
        sizeof(storage->settings->terminal.wifiMac), "%s", ap->mac);
    storage->settings->terminal.wifiEnc = ap->secMode;
    snprintf(storage->settings->terminal.wifiPwd, 
        sizeof(storage->settings->terminal.wifiPwd), "%s", pwd);
    storage->settings->terminal.netRoute = NET_ROUTE_WIFI;
    OOP_CALL(getNetwork(), setRoute, NET_ROUTE_WIFI);
    SAVE_SETTINGS();
}

STATE_DEF_HANDLE(WifiConnect, WifiEvent) {
    if (connectState == WIFI_DISCONNECT_STATE) {
        Input * in = (Input*)getState(STATE_ID_INPUT);
        wifi->connect(selectedAp, in->input);
        connectState = WIFI_CONNECT_STATE;
    } else {
        if (ev->connectStatus == WIFI_CONNECT_SUCCEED) {
            GOTO_INFO(state->parent, state->parent, "اتصال برقرار شد", "");
            Input * in = (Input*)getState(STATE_ID_INPUT);
            saveWifiInfo(selectedAp, in->input);
        } else {
            GOTO_INFO(state->parent, state->parent, "اتصال برقرار نشد", "");
        }
    }
}

static void WifiConnect(State *parent) {
    wifiConnect = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, wifiConnect, parent, "wifi connect");
    wifiConnect->vtable.enter = STATE_ENTER(WifiConnect);
    wifiConnect->vtable.exit = STATE_EXIT(WifiConnect);
    wifiConnect->vtable.handleWifi = STATE_HANDLE(WifiConnect, WifiEvent);
}

/******************** Wifi Enter pass sub state **********************/

STATE_DEF_ENTER(WifiEnterPass) {
    Input * in = (Input*)getState(STATE_ID_INPUT);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, state->parent);
    OOP_CALL(getState(STATE_ID_INPUT), setNext, wifiConnect);
    in->reset();
    in->setMode(IN_MODE_ALPHAB);
    in->setData("wifi رمز", "");
    in->setMax(32);
    SM_GOTO(getState(STATE_ID_INPUT));
}

STATE_DEF_EXIT(WifiEnterPass) {

}

static void WifiEnterPass(State *parent) {
    wifiEnterPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, wifiEnterPass, parent, "wifi enter pass");
    wifiEnterPass->vtable.enter = STATE_ENTER(WifiEnterPass);
    wifiEnterPass->vtable.exit = STATE_EXIT(WifiEnterPass);
}

/******************** Wifi scan sub state **********************/

static Menu wifiMenu;

STATE_DEF_ENTER(WifiScan) {
    SHOW_INFO("wifi جستجوی", "لطفا منتظر بمانید");
    wifi->startScan();
}

STATE_DEF_EXIT(WifiScan) {
    uiDeleteMenu(&wifiMenu);
}

STATE_DEF_HANDLE(WifiScan, KeypadEvent) {
    if (wifi->scanSt == WIFI_SCAN_UNDER_PROCESS) {
        return;
    } else if (wifi->scanSt == WIFI_SCAN_FAILED) {
    } else {
        OOP_CALL(&wifiMenu, handleItem, ev->key);
        if (ev->key == KEY_ESC) {
            OOP_CALL(&wifiMenu, hide);
            SM_GOTO(state->parent);
        } else if (ev->key == KEY_ENTER) {
            OOP_CALL(&wifiMenu, hide);
            SM_GOTO(wifiEnterPass);
            selectedAp = &wifi->apList.list[wifiMenu.idx];
        }
    }
}

STATE_DEF_HANDLE(WifiScan, WifiEvent) {
    if (ev->scanStatus == WIFI_SCAN_SUCCEED) {
        uiMenu(&wifiMenu, getDisplay()->screen);
        for (uint8_t i = 0; i < wifi->apList.size ; i++) {
            OOP_CALL(&wifiMenu, addItem, wifi->apList.list[i].essid, wifiEnterPass, NULL, NULL);
        }
        HIDE_INFO();
        OOP_CALL(&wifiMenu, show);
    } else if (ev->scanStatus == WIFI_SCAN_FAILED) {
        GOTO_INFO(state->parent, state->parent, "wifi خطا در جستجوی", "");
    }
}

static void WifiScan(State *parent) {
    wifiScan = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, wifiScan, parent, "wifi Scan");
    wifiScan->vtable.enter = STATE_ENTER(WifiScan);
    wifiScan->vtable.exit = STATE_EXIT(WifiScan);
    wifiScan->vtable.handleWifi = STATE_HANDLE(WifiScan, WifiEvent);
    wifiScan->vtable.handleKeypad = STATE_HANDLE(WifiScan, KeypadEvent);
}

/******************** Cellular connect sub state **********************/

STATE_DEF_ENTER(CellularLogin) {
    SHOW_INFO("در حال اتصال به شبکه", "لطفا منتظر بمانید");
    if (OOP_CALL(cel, getSimStatus) != CELL_ERR_OK) {
        GOTO_INFO(state->parent, state->parent, "خطا در اتصال", "وضعیت سیم کارت را بررسی کنید");
        return;
    }
    cel->startPPPlogin(NULL, NULL, NULL, NULL);
}

STATE_DEF_EXIT(CellularLogin) {
}

STATE_DEF_HANDLE(CellularLogin, CellEvent) {
    if (ev->pppSt == CELL_PPP_SUCESS) {
        GOTO_INFO(state->parent, state->parent, "با موفقیت متصل شد", "");
    } else if (ev->pppSt == CELL_PPP_FAILURE) {
        GOTO_INFO(state->parent, state->parent, "خطا در اتصال", "");
    } else if (ev->pppSt == CELL_PPP_INVALID) {
        GOTO_INFO(state->parent, state->parent, "خطا در اتصال", "");
    }
}

STATE_DEF_HANDLE(CellularLogin, KeypadEvent) {
}

static void CellularLogin(State *parent) {
    cellularLogin = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, cellularLogin, parent, "cellular login");
    cellularLogin->vtable.enter = STATE_ENTER(CellularLogin);
    cellularLogin->vtable.exit = STATE_EXIT(CellularLogin);
    cellularLogin->vtable.handleKeypad = STATE_HANDLE(CellularLogin, KeypadEvent);
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
int menuCount = 0;

static const char* itemTxt[CONNECTION_ALL] = {
    "وایفای",
    "gprs",
    "dial up",
};

static Menu menu;

static void createUi() {
    uiMenu(&menu, getDisplay()->screen);
    menuCount = 0;
    NetRoute_t route = OOP_CALL(net, getRoute);
    if (getDevice()->module.wifi) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_WIFI], wifiScan, NULL, NULL);
        menuMap[menuCount] = CONNECTION_WIFI;
        if (route == NET_ROUTE_WIFI) {
            OOP_CALL(&menu, setChecked, menuCount);
        }
        menuCount++;
    }
    if (getDevice()->module.gprs) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_GPRS], cellularLogin, NULL, NULL);
        menuMap[menuCount] = CONNECTION_GPRS;
        if (route == NET_ROUTE_CELLUALR) {
            OOP_CALL(&menu, setChecked, menuCount);
        }
        menuCount++;
    }
}

STATE_DEF_ENTER(Connectios) {
    createUi();
    GOTO_MENU(state->parent, &menu, NULL);
}

OOP_CTOR(Connections, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, "connections");
    self->base.vtable.enter = STATE_ENTER(Connectios);

    WifiScan(self);
    WifiConnect(self);
    WifiEnterPass(self);
    CellularLogin(self);

    wifi = getWifi();
    storage = getStorage();
    cel = getCell();
    net = getNetwork();
}