#include "supervisor.h"
#include "common.h"
#include "states/states.h"
#include "phrases/phrases.h"
#include "display/display.h"
#include "sys/sys.h"
#include "iso8583.h"
#include "utility/utility.h"
#include "network/network.h"
#include "isoBuilder.h"
#include "nth/nth.h"
#include "settings/settings.h"
#include "ui/menu.h"
#include "ui/infoPage.h"
#include "txnOrchestrator/txnFLow.h"

OOP_CLASS(LogOn) {
    OOP_EXTENDS(State);
    TxnFlow flow;
};

OOP_CLASS(GetMerchantData) {
    OOP_EXTENDS(State);
};

static LogOn *logOn;
static GetMerchantData *getMerchantData;
static NthTransaction *tx;

STATE_DEF_ENTER(LogOn) {
    DEFINE_STRING(ip, 32);

    normalizeIp(settings()->server.mainServerIp, ip,
                    sizeof(ip));

    txnRun(&((LogOn*)state)->flow, state, ip,
        settings()->server
            .mainServerPort, &logOnTxn);
}

/******************** Get merchant data sub state **********************/

STATE_DEF_ENTER(GetMerchantData) {

}

/******************** Configuration sub state **********************/

static Menu *cfgMenu;

STATE_DEF_ENTER(Configuration) {
    ui_menu_create(cfgMenu, disp()->screen);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_KEY), logOn, NULL, NULL);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_MERCHANT_DATA), getMerchantData, NULL, NULL);
    GOTO_MENU(state->parent, cfgMenu, NULL, NULL);
}

OOP_CTOR(Configuration, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Configuration);

    logOn = (LogOn *)MEM_ALLOC(sizeof(LogOn));
    OOP_CALL_CTOR(State, &logOn->base, self, "");
    logOn->base.vtable.enter = STATE_ENTER(LogOn);
    // logOn->base.vtable.onSocketConnect = STATE_HANDLE(LogOn, SocketConnectEvent);
    // logOn->base.vtable.onSocketSent = STATE_HANDLE(LogOn, SocketSentEvent);
    // logOn->base.vtable.onSocketReadyRead = STATE_HANDLE(LogOn, SocketReadyReadEvent);
    // logOn->base.vtable.onSocketTimeOut = STATE_HANDLE(LogOn, SocketTimeOutEvent);

    getMerchantData = (GetMerchantData *)MEM_ALLOC(sizeof(GetMerchantData));
    OOP_CALL_CTOR(State, &getMerchantData->base, self, "");
    getMerchantData->base.vtable.enter = STATE_ENTER(GetMerchantData);

    cfgMenu = (Menu *)MEM_ALLOC(sizeof(Menu));
}