#include "services.h"
#include "states/states.h"
#include "sys/sys.h"
#include "ui/menu.h"
#include "phrases/phrases.h"
#include "common.h"
#include "utility/utility.h"
#include "settings/settings.h"
#include "txnOrchestrator/txnFLow.h"
#include "ped/ped.h"

static SubState* enterPin;
static SubState* checkPin;
static SubState* commu;
static SubState* result;

static TxnFlow* flow;

static void balanceDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st);
    SM_GOTO(result);
}

const TxnFlowConfig balanceTxn = {

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_BALANCE,

    .build = buildCommon,

    .parse = parseCommon,

    .done = balanceDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

STATE_DEF_ENTER(Balance) { SM_GOTO(enterPin); }

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPin) {
    DEFINE_STRING(wage, 56);
    DEFINE_STRING(amnt, 16);
    DEFINE_STRING(amntSep, 32);
    intToStr(settings()->txn.balanceInqWage, amnt, sizeof(amnt));
    amountSeparator(amnt, amntSep, sizeof(amntSep));
    snprintf(wage, sizeof(wage), "%s %s %s", phraseGetDef(PHRASE_RIAL), amntSep,
             phraseGetDef(PHRASE_WAGE));
    PedErr_t err = OOP_CALL(ped(), enterPinEntryMode);
    GOTO_INPUT(STATE_IDLE, commu, phraseGetDef(PHRASE_CARD_PIN), wage,
               PASSWORD_MAX_LEN, IN_MODE_PASSWORD, NULL);
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Communication) {
    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(flow, state, ip, settings()->server.mainServerPort, &balanceTxn);
}

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) { GOTO_TXN_RES(STATE_IDLE, STATE_IDLE); }

/******************************************************************/

OOP_CTOR(Balance, State* parent, const char* name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Balance);

    enterPin = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPin, &self->base.state, "enter password");
    enterPin->vtable.enter = STATE_ENTER(EnterPin);

    commu = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, commu, &self->base.state, "communication");
    commu->vtable.enter = STATE_ENTER(Communication);

    result = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &self->base.state, "result");
    result->vtable.enter = STATE_ENTER(Result);

    flow = &self->base.flow;
}