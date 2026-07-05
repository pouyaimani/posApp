#include "services.h"
#include "states/states.h"
#include "sys/sys.h"
#include "iso/iso8583.h"

static SubState* selectOperator;
static SubState* selectAmount;
static SubState* enterPass;
static SubState* communication;
static SubState* result;

STATE_DEF_ENTER(TopUp) { SM_GOTO(enterPass); }

STATE_DEF_EXIT(TopUp) {}
/******************** Select Operator sub state **********************/

STATE_DEF_ENTER(SelectOperator) {}

STATE_DEF_EXIT(SelectOperator) {}

static void SelectOperator(Sale* parent) {
    selectOperator = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, selectOperator, &parent->base.state,
                  "select operator");
    selectOperator->vtable.enter = STATE_ENTER(SelectOperator);
    selectOperator->vtable.exit  = STATE_EXIT(SelectOperator);
}

/******************************************************************/

/******************** Select amount sub state **********************/

STATE_DEF_ENTER(SelectAmount) {}

STATE_DEF_EXIT(SelectAmount) {}

static void SelectAmount(Sale* parent) {
    selectAmount = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, selectAmount, &parent->base.state, "select amount");
    selectAmount->vtable.enter = STATE_ENTER(SelectAmount);
    selectAmount->vtable.exit  = STATE_EXIT(SelectAmount);
}

/******************************************************************/

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {}

STATE_DEF_EXIT(EnterPassword) {}

static void EnterPassword(Sale* parent) {
    enterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &parent->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);
    enterPass->vtable.exit  = STATE_EXIT(EnterPassword);
}

/******************************************************************/

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) {}

STATE_DEF_EXIT(Result) {}

static void Result(Sale* parent) {
    result = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &parent->base.state, "Result");
    result->vtable.enter = STATE_ENTER(Result);
    result->vtable.exit  = STATE_EXIT(Result);
}

static void topupDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        // settings()->save();
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

const TxnFlowConfig topupTxn = {

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .build = buildCommon,

    .parse = parseCommon,

    .done = topupDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

/******************************************************************/

OOP_CTOR(TopUp, State* parent, const char* name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(TopUp);
    self->base.state.vtable.exit  = STATE_EXIT(TopUp);

    SelectOperator(self);
    SelectAmount(self);
    EnterPassword(self);
    Result(self);
}