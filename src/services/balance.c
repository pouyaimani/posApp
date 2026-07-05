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
#include "input/inputMgr.h"
#include "iso/iso8583.h"

static SubState* enterPin;
static SubState* checkPin;
static SubState* commu;
static SubState* result;

static TxnFlow* flow;

STATE_DEF_ENTER(Balance) { SM_GOTO(enterPin); }

/******************** Enter pass sub state **********************/

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
            .maxLen = CARD_PIN_LEN,
        },
        STATE_IDLE, commu);
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Communication) {
    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(flow, state, ip, settings()->server.mainServerPort, &balanceTxn);
}

/*********************** Result sub state *************************/

// int8_t makeReceipt(TxnData* txn) {
//     RETURN_VALUE_IF_NULL(txn, ;, ERR_NOK);
//     Receipt rec;
//     int8_t  ret = buildReceipt(&rec, txn);
//     RETURN_VALUE_IF_NOT(
//         ret, ERR_OK,
//         {
//             RECEIPT_CREATE_ERROR();
//             OOP_CALL(&rec, destroy);
//         },
//         ret);
//     OOP_CALL(&rec, flush);
//     OOP_CALL(&rec, destroy);
//     return ERR_OK;
// }

static void balanceDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
    // SM_GOTO(result);
    // &flow->data
}

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

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_BALANCE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .build = buildCommon,

    .parse = parseCommon,

    .done = balanceDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

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

    flow = &self->base.flow;
}