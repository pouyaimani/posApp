#include "supervisor.h"
#include "common.h"
#include "states/states.h"
#include "phrases/phrases.h"
#include "display/display.h"
#include "sys/sys.h"
#include "utility/utility.h"
#include "network/network.h"
#include "settings/settings.h"
#include "ui/menu.h"
#include "ui/infoPage.h"
#include "txnFlow/txnFLow.h"

OOP_CLASS(LogOn) {
    OOP_EXTENDS(State);
    TxnFlow flow;
};

OOP_CLASS(GetMerchantData) {
    OOP_EXTENDS(State);
    TxnFlow flow;
};

static LogOn*           logOn;
static GetMerchantData* getMerchantData;

STATE_DEF_ENTER(LogOn) {
    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(&((LogOn*)state)->flow, state, ip, settings()->server.mainServerPort,
           &logOnTxn);
}

/******************** Get merchant data sub state **********************/

STATE_DEF_ENTER(GetMerchantData) {
    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(&((LogOn*)state)->flow, state, ip, settings()->server.mainServerPort,
           &cfgTxn);
}

/******************** Configuration sub state **********************/

static Menu* cfgMenu;

STATE_DEF_ENTER(Configuration) {
    ui_menu_create(cfgMenu, disp()->screen);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_KEY), logOn,
                    LV_TEXT_ALIGN_RIGHT, NULL, NULL);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_MERCHANT_DATA),
                    LV_TEXT_ALIGN_RIGHT, getMerchantData, NULL, NULL);
    GOTO_MENU(state->parent, cfgMenu, NULL, NULL);
}

OOP_CTOR(Configuration, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Configuration);

    logOn = (LogOn*)MEM_ALLOC(sizeof(LogOn));
    OOP_CALL_CTOR(State, &logOn->base, self, "");
    logOn->base.vtable.enter = STATE_ENTER(LogOn);

    getMerchantData = (GetMerchantData*)MEM_ALLOC(sizeof(GetMerchantData));
    OOP_CALL_CTOR(State, &getMerchantData->base, self, "");
    getMerchantData->base.vtable.enter = STATE_ENTER(GetMerchantData);

    cfgMenu = (Menu*)MEM_ALLOC(sizeof(Menu));
}