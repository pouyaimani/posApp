#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "wifi/wifi.h"

static Wifi *wifi; 
static SubState *wifiScan;
static SubState *wifiConnect;
<<<<<<< HEAD
=======
static SubState *wifiEnterPass;
>>>>>>> 060e86f1e9bbb30d2a0b5fd14f8ec6352f591b1e

WifiApInfo_t *chosenAp;

/******************** Wifi connect sub state **********************/

STATE_DEF_ENTER(WifiConnect) {
<<<<<<< HEAD
}

STATE_DEF_EXIT(WifiConnect) {

}

STATE_DEF_HANDLE(WifiConnect, KeypadEvent) {
}

STATE_DEF_HANDLE(WifiConnect, WifiEvent) {
=======
    Input * in = (Input*)getState(STATE_ID_INPUT);
    InfoPage info = infoPage();
    OOP_CALL(&info, show);
    OOP_CALL(&info, setData, INFO_T_TEXT, "در حال اتصال به wifi", "لطفا منتظر بمانید");
    wifi->connect(chosenAp, in->input);
}

STATE_DEF_EXIT(WifiConnect) {
    
}

STATE_DEF_HANDLE(WifiConnect, WifiEvent) {
    Info *info = (Info *)getState(STATE_ID_INFO);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, state->parent);
    OOP_CALL(getState(STATE_ID_INPUT), setNext, state->parent);
    if (ev->connectStatus == WIFI_CONNECT_SUCCEED) {
        info->setText("اتصال برقرار شد", "");
    } else {
        info->setText("اتصال برقرار نشد", "");
    }
    SM_GOTO(getState(STATE_ID_INFO));
>>>>>>> 060e86f1e9bbb30d2a0b5fd14f8ec6352f591b1e
}

static void WifiConnect(State *parent) {
    wifiConnect = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, wifiConnect, parent, "wifi connect");
    wifiConnect->vtable.enter = STATE_ENTER(WifiConnect);
    wifiConnect->vtable.exit = STATE_EXIT(WifiConnect);
    wifiConnect->vtable.handleWifi = STATE_HANDLE(WifiConnect, WifiEvent);
<<<<<<< HEAD
    wifiConnect->vtable.handleKeypad = STATE_HANDLE(WifiConnect, KeypadEvent);
=======
}

/******************** Wifi Enter pass sub state **********************/

STATE_DEF_ENTER(WifiEnterPass) {
    Input * in = (Input*)getState(STATE_ID_INPUT);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, state->parent);
    OOP_CALL(getState(STATE_ID_INPUT), setNext, wifiConnect);
    in->reset();
    in->setMode(IN_MODE_ALPHAB);
    in->setData("رمز wifi", "");
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
>>>>>>> 060e86f1e9bbb30d2a0b5fd14f8ec6352f591b1e
}

/******************** Wifi scan sub state **********************/

static Menu wifiMenu;

STATE_DEF_ENTER(WifiScan) {
    InfoPage info = infoPage();
    OOP_CALL(&info, show);
    OOP_CALL(&info, setData, INFO_T_TEXT, "جستجوی wifi", "لطفا منتظر بمانید");
    wifi = getWifi();
    wifi->startScan();
}

STATE_DEF_EXIT(WifiScan) {

}

STATE_DEF_HANDLE(WifiScan, KeypadEvent) {
    if (wifi->scanSt == WIFI_SCAN_UNDER_PROCESS) {
        return;
    } else if (wifi->scanSt == WIFI_SCAN_FAILED) {

    } else {
        OOP_CALL(&wifiMenu, handleItem, ev->key);
        if (ev->key == KEY_ESC) {
            SM_GOTO(getState(state->parent));
        } else if (ev->key == KEY_ENTER) {
<<<<<<< HEAD
            SM_GOTO(getState(wifiConnect));
=======
            SM_GOTO(getState(wifiEnterPass));
>>>>>>> 060e86f1e9bbb30d2a0b5fd14f8ec6352f591b1e
            chosenAp = &wifi->apList.list[wifiMenu.idx];
        }
    }
}

STATE_DEF_HANDLE(WifiScan, WifiEvent) {
<<<<<<< HEAD
    if (wifi->scanSt == WIFI_SCAN_SUCCEED) {
=======
    if (ev->scanStatus == WIFI_SCAN_SUCCEED) {
>>>>>>> 060e86f1e9bbb30d2a0b5fd14f8ec6352f591b1e
        wifiMenu = uiMenu(getDisplay()->screen);
        for (uint8_t i = 0; i < wifi->apList.size ; i++) {
            OOP_CALL(&wifiMenu, addItem, wifi->apList.list[i].essid, NULL, NULL);
        }
<<<<<<< HEAD
    } else if (wifi->scanSt == WIFI_SCAN_FAILED) {
=======
    } else if (ev->scanStatus == WIFI_SCAN_FAILED) {
>>>>>>> 060e86f1e9bbb30d2a0b5fd14f8ec6352f591b1e
        InfoPage info = infoPage();
        OOP_CALL(&info, show);
        OOP_CALL(&info, setData, INFO_T_TEXT, "خطا در جستجوی wifi", "");
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
    menu = uiMenu(getDisplay()->screen);
    menuCount = 0;
    if (getDevice()->module.wifi) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_WIFI], NULL, NULL);
        menuMap[menuCount++] = CONNECTION_WIFI;
    }
    if (getDevice()->module.gprs) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_GPRS], NULL, NULL);
        menuMap[menuCount++] = CONNECTION_GPRS;
    }
    if (getDevice()->module.dialup) {
        OOP_CALL(&menu, addItem, itemTxt[CONNECTION_DIAL], NULL, NULL);
        menuMap[menuCount++] = CONNECTION_DIAL;
    }       
}

static void destroyUi() {
    uiDeleteMenu(&menu);
}

STATE_DEF_ENTER(Connectios) {
    createUi();
    OOP_CALL(&menu, show);
}

STATE_DEF_EXIT(Connectios) {
    OOP_CALL(&menu, hide);
    destroyUi();
}

static void handleKeyAction(State *state, int id) {
    if (id >= menuCount) {
        return;
    }
    switch (menuMap[id]) {
    case CONNECTION_WIFI:
        SM_GOTO(wifiScan);
        break;
    default:
        break;
    }
}

STATE_DEF_HANDLE(Connectios, KeypadEvent) {
    OOP_CALL(&menu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        handleKeyAction(state, menu.idx);
    }  else {
        if (ev->key <= KEY_9) {
            int id = ((int)ev->key - 1);
            handleKeyAction(state, id);
        }
    }
}

OOP_CTOR(Connections, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, "connections");
    self->base.vtable.enter = STATE_ENTER(Connectios);
    self->base.vtable.exit = STATE_EXIT(Connectios);
    self->base.vtable.handleKeypad = STATE_HANDLE(Connectios, KeypadEvent);

    WifiScan(self);
    WifiConnect(self);
<<<<<<< HEAD
=======
    WifiEnterPass(self);
>>>>>>> 060e86f1e9bbb30d2a0b5fd14f8ec6352f591b1e
}