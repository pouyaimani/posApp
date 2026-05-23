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

static SubState *getKey;
static SubState *getMerchantData;

/******************** Get Key sub state **********************/

static int8_t onGetKeySuc(void *userData) {
    ByteArray *packet = (ByteArray *)userData;
    MEM_FREE(packet->data);
    MEM_FREE(packet);
}

static int8_t onGetKeyFail(void *userData) {
    ByteArray *packet = (ByteArray *)userData;
    MEM_FREE(packet->data);
    MEM_FREE(packet);
}

static int8_t createIso(void *userData) {
    return isoBuild(MTI_LOG_ON, (ByteArray*)userData);
}

STATE_DEF_ENTER(GetKey) {
    IsoTransmitter *transmitter = (IsoTransmitter *)STATE_TRNS_ISO;
    NetConnect *con = (NetConnect *)transmitter->connect;
    con->ctx.onFailure = state->parent;
    NetSubTaskCtx_t connect = {0};
    NetSubTaskCtx_t send = {0};
    NetSubTaskCtx_t rec = {0};
    connect.onFailure = state->parent;
    connect.onFailureCb = onGetKeyFail;
    connect.onSucessCb = createIso;
    connect.userDataOnSucess = &((NetSend *)transmitter->send)->data;
    send.onFailure = state->parent;
    send.onFailureCb = onGetKeyFail;
    rec.onFailure = state->parent;
    rec.onFailureCb = onGetKeyFail;
    rec.onSucess = state->parent;
    rec.onSucessCb = onGetKeySuc;
    rec.userDataOnFailure = &((NetReceive *)transmitter->receive)->data;
    rec.userDataOnSucess = &((NetReceive *)transmitter->receive)->data;
    GOTO_ISO_TRANSMITTER(&connect, &send, &rec);
}

/******************** Get merchant data sub state **********************/

STATE_DEF_ENTER(GetMerchantData) {

}

/******************** Configuration sub state **********************/

static Menu *cfgMenu;

STATE_DEF_ENTER(Configuration)
{
    ui_menu_create(cfgMenu, disp()->screen);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_KEY), getKey, NULL, NULL);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_MERCHANT_DATA), getMerchantData, NULL, NULL);
    GOTO_MENU(state->parent, cfgMenu, NULL, NULL);
}

OOP_CTOR(Configuration, State *parent, const char *name)
{
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Configuration);

    getKey = (State *)MEM_ALLOC(sizeof(State));
    OOP_CALL_CTOR(State, getKey, self, "");
    getKey->vtable.enter = STATE_ENTER(GetKey);
    getMerchantData = (State *)MEM_ALLOC(sizeof(State));
    OOP_CALL_CTOR(State, getMerchantData, self, "");
    getMerchantData->vtable.enter = STATE_ENTER(GetMerchantData);
}