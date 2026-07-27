#include "transaction.h"
#include "states/states.h"
#include "sys/sys.h"
#include "iso/iso8583.h"
#include "phrases/phrases.h"
#include "input/inputMgr.h"
#include "txn.h"
#include "ui/infoPage.h"
#include "common.h"

/******************************************************************
 *                           Substates
 ******************************************************************/

static SubState* enterBillId;
static SubState* enterPayId;
static SubState* enterPass;
static SubState* communication;

/******************************************************************
 *               Global variable within this file
 ******************************************************************/

static TxnFlow*            flow;
extern const TxnFlowConfig billTxn;
static TxnExtention        data;

STATE_DEF_ENTER(Bill) {
    memset(flow, 0, sizeof(*flow));
    SM_GOTO(enterBillId);
}

/******************************************************************
 *                      Enter Bill id sub state
 ******************************************************************/

STATE_DEF_ENTER(EnterBillId) {
    inmgr()->run(
        &(InputCfg){
            .mode   = INMD_ENTER_NUMBERS,
            .title  = phraseGetDef(PHRASE_BILL_ID),
            .info   = "",
            .maxLen = LEN_BILL_ID_MAX,
        },
        STATE_IDLE, enterPayId);
    inmgr()->setOut(data.bill.billId, NULL, sizeof(data.bill.billId));
}

/******************************************************************
 *                      Enter pay id sub state
 ******************************************************************/

STATE_DEF_ENTER(EnterPayId) {
    if (!isBillIdValid(data.bill.billId)) {
        GOTO_INFO(STATE_IDLE, STATE_IDLE, INFO_ERROR,
                  phraseGetDef(PHRASE_BILL_INVALID), "");
        return;
    }
    inmgr()->run(
        &(InputCfg){
            .mode   = INMD_ENTER_NUMBERS,
            .title  = phraseGetDef(PHRASE_BILL_ID),
            .info   = "",
            .maxLen = LEN_PAYMENT_ID_MAX,
        },
        STATE_IDLE, enterPass);
    inmgr()->setOut(data.bill.paymentId, NULL, sizeof(data.bill.paymentId));
}

/******************************************************************
 *                      Enter pass sub state
 ******************************************************************/
static uint64_t amount;

STATE_DEF_ENTER(EnterPassword) {
    // if (strlen(inmgr()->input) < LEN_MIN_AMOUNT) {
    //     GOTO_INFO(enterAmount, enterAmount, INFO_ERROR,
    //               phraseGetDef(PHRASE_AMOUNT_FLOOR_ER), "");
    //     return;
    // }

    if (!isBillValid(data.bill.billId, data.bill.paymentId)) {
        GOTO_INFO(STATE_IDLE, STATE_IDLE, INFO_ERROR,
                  phraseGetDef(PHRASE_BILL_INVALID), "");
        return;
    }
    DEFINE_STRING(amountStr, LEN_MAX_AMOUNT);
    billExtractAmount(data.bill.paymentId, amountStr, sizeof(amountStr));

    if (!str2u64(amountStr, &amount)) {
        LOG_FATAL("Converting amount form string to u64 failed.");
    }
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_PED,
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_CARD_PIN),
            .info   = phraseGetDef(PHRASE_BY_CUSTOMER),
            .maxLen = LEN_MAX_CARD_PIN,
        },
        STATE_IDLE, communication);
}

/******************************************************************
 *                Communication sub state
 ******************************************************************/

STATE_DEF_ENTER(Communication) { txnStart(&billTxn, flow, state); }

/******************************************************************
 *                      Result sub state
 ******************************************************************/

static int compose(TxnData* txnData) {
    composeCommon(txnData);
    txnData->core.amount    = amount;
    txnData->extention.bill = data.bill;
}

static void billDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
    }
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
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

    .compose = compose,

    .build = buildCommon,

    .parse = parseCommon,

    .done = billDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

OOP_CTOR(Bill, State* parent, const char* name) {
    OOP_CALL_CTOR(Transaction, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Bill);

    enterBillId = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterBillId, &self->base.state, "enter bill id");
    enterBillId->vtable.enter = STATE_ENTER(EnterBillId);

    enterPayId = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPayId, &self->base.state, "enter pay id");
    enterPayId->vtable.enter = STATE_ENTER(EnterPayId);

    enterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &self->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    communication = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, communication, &self->base.state, "enter password");
    communication->vtable.enter = STATE_ENTER(Communication);

    flow = self->base.flow;
}