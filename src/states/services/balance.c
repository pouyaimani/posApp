#include "services.h"
#include "states/states.h"
#include "sys/sys.h"
#include "ui/menu.h"
#include "phrases/phrases.h"
#include "common.h"
#include "utility/utility.h"
#include "settings/settings.h"
#include "txnOrchestrator/txnFLow.h"

static SubState* enterPass;
static SubState* commu;
static SubState* result;

STATE_DEF_ENTER(Balance) { SM_GOTO(enterPass); }

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {
    DEFINE_STRING(wage, 56);
    DEFINE_STRING(amnt, 16);
    DEFINE_STRING(amntSep, 32);
    intToStr(settings()->txn.balanceInqWage, amnt, sizeof(amnt));
    amountSeparator(amnt, amntSep, sizeof(amntSep));
    snprintf(wage, sizeof(wage), "%s %s %s", phraseGetDef(PHRASE_RIAL), amntSep,
             phraseGetDef(PHRASE_WAGE));
    GOTO_INPUT(STATE_IDLE, commu, phraseGetDef(PHRASE_CARD_PIN), wage,
               PASSWORD_MAX_LEN, IN_MODE_PASSWORD, NULL);
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Communication) {
    // DEFINE_STRING(ip, 32);
    // normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    // txnRun(&((LogOn*)state)->flow, state, ip,
    // settings()->server.mainServerPort,
    //        &cfgTxn);
}

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) { GOTO_TXN_RES(STATE_IDLE, STATE_IDLE); }

/******************************************************************/

OOP_CTOR(Balance, State* parent, const char* name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Balance);

    enterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &self->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    commu = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, commu, &self->base.state, "communication");
    commu->vtable.enter = STATE_ENTER(Communication);

    result = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &self->base.state, "result");
    result->vtable.enter = STATE_ENTER(Result);
}