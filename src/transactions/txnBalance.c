#include "transaction.h"
#include "states/states.h"
#include "sys/sys.h"
#include "phrases/phrases.h"
#include "common.h"
#include "utility/utility.h"
#include "settings/settings.h"
#include "txnFlow/txnFLow.h"
#include "ped/ped.h"
#include "input/inputMgr.h"
#include "iso/iso8583.h"
#include "receipt/receiptTemplates.h"
#include "txnCommon.h"

/******************************************************************
 *                           Substates
 ******************************************************************/

static SubState* enterPin;
static SubState* checkPin;
static SubState* commu;
static SubState* result;

/******************************************************************
 *                Global variable within this file
 ******************************************************************/

static TxnFlow* flow;

STATE_DEF_ENTER(Balance) { SM_GOTO(enterPin); }

/******************************************************************
 *                Enter pass sub state
 ******************************************************************/

STATE_DEF_ENTER(EnterPin) {
    static DEFINE_STRING(wage, 56);
    DEFINE_STRING(amnt, 16);
    DEFINE_STRING(amntSep, 32);
    intToStr(settings()->txn.balanceInqWage, amnt, sizeof(amnt));
    amountSeparator(amnt, amntSep, sizeof(amntSep));
    snprintf(wage, sizeof(wage), "%s %s %s", phraseGetDef(PHRASE_WAGE), amntSep,
             phraseGetDef(PHRASE_RIAL));
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_PED,
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_CARD_PIN),
            .info   = wage,
            .maxLen = LEN_MAX_CARD_PIN,
        },
        STATE_IDLE, commu);
}

/******************************************************************
 *                  Communication sub state
 ******************************************************************/

STATE_DEF_ENTER(Communication) {
    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(flow, state, ip, settings()->server.mainServerPort, &balanceTxn);
}

/******************************************************************
 *                  Result sub state
 ******************************************************************/

static const uint8_t isoFeilds[] = {ELEMENT_PAN,
                                    ELEMENT_PROCESSING_CODE,
                                    ELEMENT_STAN,
                                    ELEMENT_TIME_LOCAL_TRANSACTION,
                                    ELEMENT_DATE_LOCAL_TRANSACTION,
                                    ELEMENT_POS_ENTRY_MODE,
                                    ELEMENT_NETWORK_INTL_ID,
                                    ELEMENT_POS_CONDITION_CODE,
                                    ELEMENT_ACQUIRING_INSTITUTION_ID,
                                    ELEMENT_TRACK2,
                                    ELEMENT_TERMINAL_ID,
                                    ELEMENT_CARD_ACCEPTOR_ID,
                                    ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                    ELEMENT_PIN_DATA,
                                    ELEMENT_SECURITY_CONTROL_INFO,
                                    ELEMENT_MAC};

const TxnFlowConfig balanceTxn = {

    TXN_FLOW_COMMON,

    .type = TXN_BALANCE,

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_BALANCE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .compose = composeCommon,

    .done = financeTxnDone,

    .needSettlement = false};

OOP_CTOR(Balance, State* parent, const char* name) {
    OOP_CALL_CTOR(Transaction, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Balance);

    enterPin = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPin, &self->base.state, "enter password");
    enterPin->vtable.enter = STATE_ENTER(EnterPin);

    commu = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, commu, &self->base.state, "communication");
    commu->vtable.enter = STATE_ENTER(Communication);

    flow = self->base.flow;
}