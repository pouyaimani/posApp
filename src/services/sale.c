#include "services.h"
#include "states/states.h"
#include "sys/sys.h"
#include "ui/menu.h"
#include "iso8583.h"
#include "receipt/receiptTemplates.h"
#include "settings/settings.h"
#include "txn.h"
#include "utility/utility.h"
#include "phrases/phrases.h"
#include "input/inputMgr.h"
#include "iso/iso8583.h"

static SubState* enterAmount;
static SubState* enterPass;
static SubState* commu;
static SubState* result;

STATE_DEF_ENTER(Sale) { SM_GOTO(enterAmount); }

/******************** Enter amount sub state **********************/

static char* amount;

STATE_DEF_ENTER(EnterAmount) {
    inmgr()->run(
        &(InputCfg){
            .mode   = INMD_ENTER_AMOUNT,
            .title  = phraseGetDef(PHRASE_AMOUNT),
            .info   = "",
            .maxLen = AMOUNT_MAX_CNT,
        },
        STATE_IDLE, commu);
}

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {
    // Input* in = getState(STATE_ID_INPUT);
    // OOP_CALL(packer(), setAmount, in->input);
    inmgr()->run(
        &(InputCfg){
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_CARD_PIN),
            .info   = "",
            .maxLen = PASSWORD_MAX_LEN,
        },
        STATE_IDLE, commu);
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Communication) {}

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) { GOTO_TXN_RES(STATE_IDLE, STATE_IDLE); }

/******************************************************************/

int8_t makeReceipt(TxnData* txn) {
    RETURN_VALUE_IF_NULL(txn, ;, ERR_NOK);
    Receipt rec;
    int8_t  ret = buildReceipt(&rec, txn);
    RETURN_VALUE_IF_NOT(
        ret, ERR_OK,
        {
            RECEIPT_CREATE_ERROR();
            OOP_CALL(&rec, destroy);
        },
        ret);
    OOP_CALL(&rec, flush);
    OOP_CALL(&rec, destroy);
    return ERR_OK;
}

static void purchaseDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
    // SM_GOTO(result);
    // &flow->data
}

static const uint8_t isoFeilds[] = {ELEMENT_PAN,
                                    ELEMENT_PROCESSING_CODE,
                                    ELEMENT_AMOUNT_TRANSACTION,
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
                                    ELEMENT_CURRENCY_CODE_TRANSACTION,
                                    ELEMENT_PIN_DATA,
                                    ELEMENT_SECURITY_CONTROL_INFO,
                                    ELEMENT_MAC};

const TxnFlowConfig purchaseTxn = {

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .build = buildCommon,

    .parse = parseCommon,

    .done = purchaseDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

OOP_CTOR(Sale, State* parent, const char* name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Sale);
    self->base.vtable.makeReceipt = makeReceipt;

    enterAmount = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, &self->base.state, "enter Amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterAmount);

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