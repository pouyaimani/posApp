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
#include "transmitter.h"
#include "nth/nth.h"
#include "settings/settings.h"
#include "ui/menu.h"
#include "ui/infoPage.h"

OOP_CLASS(LogOn) {
    OOP_EXTENDS(State);
};

OOP_CLASS(GetMerchantData) {
    OOP_EXTENDS(State);
};

static LogOn *logOn;
static GetMerchantData *getMerchantData;
static NthTransaction *tx;

/******************** Get Key sub state **********************/

STATE_DEF_HANDLE(LogOn, SocketConnectEvent) {
    ByteArray ba;
    if (!ev->isConnected) {
        GOTO_INFO(state->parent, 
            state->parent, phraseGetDef(PHRASE_CONNECTION_ERR), "");
        return;
    }
    byteArrayInit(&ba, NT_TX_BUFFER_SIZE);
    if (isoBuild(MTI_LOG_ON, &ba) != ERR_OK) {
        SM_GOTO(state->parent);
        LOG_DEBUG("Building iso failed...");
        return;
    }
    LOG_DEBUG("Building iso succeed...");
    nth()->send(tx, &ba);
}

STATE_DEF_HANDLE(LogOn, SocketSentEvent) {

}

STATE_DEF_HANDLE(LogOn, SocketReadyReadEvent) {

}

STATE_DEF_HANDLE(LogOn, SocketTimeOutEvent) {
    SM_GOTO(state->parent);
    LOG_DEBUG("Log on is time out...");
}

STATE_DEF_ENTER(LogOn) {
    // IsoTransmitter *transmitter = (IsoTransmitter *)STATE_TRNS_ISO;
    // NetConnect *con = (NetConnect *)transmitter->connect;
    // con->ctx.onFailure = state->parent;
    // NetSubTaskCtx_t connect = {0};
    // NetSubTaskCtx_t send = {0};
    // NetSubTaskCtx_t rec = {0};
    // connect.onFailure = state->parent;
    // connect.onFailureCb = onGetKeyFail;
    // connect.onSucessCb = createIso;
    // connect.userDataOnSucess = &((NetSend *)transmitter->send)->data;
    // send.onFailure = state->parent;
    // send.onFailureCb = onGetKeyFail;
    // rec.onFailure = state->parent;
    // rec.onFailureCb = onGetKeyFail;
    // rec.onSucess = state->parent;
    // rec.onSucessCb = onGetKeySuc;
    // rec.userDataOnFailure = &((NetReceive *)transmitter->receive)->data;
    // rec.userDataOnSucess = &((NetReceive *)transmitter->receive)->data;
    // GOTO_ISO_TRANSMITTER(&connect, &send, &rec);

    tx = nth()->alloc();
    RETURN_IF_NULL(tx, 
            GOTO_INFO(state->parent, 
            state->parent, phraseGetDef(PHRASE_CONNECTION_ERR), "");
        );
    SHOW_INFO(phraseGetDef(PHRASE_CONNECTIING), "");
    tx->owner = state;
    DEFINE_STRING(ip, 24);
    LOG_DEBUG("server ip = %s", settings()->server.mainServerIp);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    LOG_DEBUG("normalize ip = %s", ip);
    NthResult res = nth()->connect(tx, ip,
                settings()->server.mainServerPort);
    if (res == NTH_ERR_INVALID_HOST) {
        GOTO_INFO(state->parent, 
            state->parent, phraseGetDef(PHRASE_INVALID_IP),
                 ip);
        nth()->release(tx);
    } else if (res != NTH_OK) {
        GOTO_INFO(state->parent, 
            state->parent, phraseGetDef(PHRASE_CONNECTION_ERR),
                    phraseGetDef(PHRASE_CHECK_NET));
        nth()->release(tx);
    }
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
    logOn->base.vtable.onSocketConnect = STATE_HANDLE(LogOn, SocketConnectEvent);
    logOn->base.vtable.onSocketSent = STATE_HANDLE(LogOn, SocketSentEvent);
    logOn->base.vtable.onSocketReadyRead = STATE_HANDLE(LogOn, SocketReadyReadEvent);
    logOn->base.vtable.onSocketTimeOut = STATE_HANDLE(LogOn, SocketTimeOutEvent);

    getMerchantData = (GetMerchantData *)MEM_ALLOC(sizeof(GetMerchantData));
    OOP_CALL_CTOR(State, &getMerchantData->base, self, "");
    getMerchantData->base.vtable.enter = STATE_ENTER(GetMerchantData);

    cfgMenu = (Menu *)MEM_ALLOC(sizeof(Menu));
}