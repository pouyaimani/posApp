#include "transaction.h"
#include "states/states.h"
#include "sys/sys.h"
#include "iso/iso8583.h"

static SubState* enterBillId;
static SubState* enterPayId;
static SubState* enterPass;
static SubState* communication;
static SubState* result;

static TxnFlow* flow;

STATE_DEF_ENTER(Bill) {
    memset(flow, 0, sizeof(*flow));
    SM_GOTO(enterPass);
}

STATE_DEF_EXIT(Bill) {}
/******************** Enter Bill id sub state **********************/

STATE_DEF_ENTER(EnterBillId) {}

STATE_DEF_EXIT(EnterBillId) {}

static void EnterBillId(Bill* parent) {
    enterBillId = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterBillId, &parent->base.state, "enter bill id");
    enterBillId->vtable.enter = STATE_ENTER(EnterBillId);
    enterBillId->vtable.exit  = STATE_EXIT(EnterBillId);
}

/******************************************************************/

/******************** Enter pay id sub state **********************/

STATE_DEF_ENTER(EnterPayId) {}

STATE_DEF_EXIT(EnterPayId) {}

static void EnterPayId(Bill* parent) {
    enterPayId = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPayId, &parent->base.state, "enter pay id");
    enterPayId->vtable.enter = STATE_ENTER(EnterPayId);
    enterPayId->vtable.exit  = STATE_EXIT(EnterPayId);
}

/******************************************************************/

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {}

STATE_DEF_EXIT(EnterPassword) {}

static void EnterPassword(Bill* parent) {
    enterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &parent->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);
    enterPass->vtable.exit  = STATE_EXIT(EnterPassword);
}

/******************************************************************/

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) {}

STATE_DEF_EXIT(Result) {}

static void Result(Bill* parent) {
    result = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &parent->base.state, "Result");
    result->vtable.enter = STATE_ENTER(Result);
    result->vtable.exit  = STATE_EXIT(Result);
}

static void billDone(TxnFlow* flow, const TxnFlowStatus* st) {
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

const TxnFlowConfig billTxn = {

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .build = buildCommon,

    .parse = parseCommon,

    .done = billDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

/******************************************************************/

OOP_CTOR(Bill, State* parent, const char* name) {
    OOP_CALL_CTOR(Transaction, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Bill);
    self->base.state.vtable.exit  = STATE_EXIT(Bill);

    EnterBillId(self);
    EnterPayId(self);
    EnterPassword(self);
    Result(self);

    flow = self->base.flow;
}