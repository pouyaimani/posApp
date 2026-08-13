#include "transaction.h"
#include "states/states.h"
#include "display/display.h"
#include "sys/sys.h"
#include "iso/iso8583.h"
#include "ui/menu.h"
#include "phrases/phrases.h"
#include "input/inputMgr.h"
#include "txnCommon.h"

/******************************************************************
 *                           Substates
 ******************************************************************/

static SubState* selectOperator;
static SubState* selectAmount;
static SubState* enterPhone;
static SubState* enterPass;
static SubState* communication;

/******************************************************************
 *                Global variable within this file
 ******************************************************************/

static Menu opSelectionMenu;
static Menu amntSelectionMenu;

static SimCardOp_t selectedOp;
static TxnType     txn;

static TxnFlow* flow;

static TxnFlowConfig* cfg;

static char phoneNum[LEN_MAX_PHONE_NUMBER + 1];

/******************************************************************
 *                   Select Operator sub state
 ******************************************************************/

void setOperator(void* arg) {
    int idx = *(((int*)arg));
    switch (idx) {
    case 0:
        selectedOp = OPERATOR_MCI;
        break;
    case 1:
        selectedOp = OPERATOR_MTN;
        break;
    case 2:
        selectedOp = OPERATOR_RIGHTEL;
        break;

    default:
        break;
    }
    SM_GOTO(selectAmount);
}

STATE_DEF_ENTER(SelectOperator) {
    ui_menu_create(&opSelectionMenu, disp()->screen);
    ui_menu_addItem(&opSelectionMenu, phraseGetDef(PHRASE_SIM_OP_MCI),
                    LV_TEXT_ALIGN_RIGHT, NULL, setOperator, NULL);
    ui_menu_addItem(&opSelectionMenu, phraseGetDef(PHRASE_SIM_OP_MTN),
                    LV_TEXT_ALIGN_RIGHT, NULL, setOperator, NULL);
    ui_menu_addItem(&opSelectionMenu, phraseGetDef(PHRASE_SIM_OP_RIGHTEL),
                    LV_TEXT_ALIGN_RIGHT, NULL, setOperator, NULL);
    GOTO_MENU(STATE_IDLE, &opSelectionMenu, NULL, NULL);
}

static void SelectOperator(State* parent) {
    selectOperator = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, selectOperator, parent, "select operator");
    selectOperator->vtable.enter = STATE_ENTER(SelectOperator);
}

/******************************************************************
 *                   Select amount sub state
 ******************************************************************/

static const uint64_t amnt[] = {
    20000,  // 0
    50000,  // 1
    100000, // 2
    200000, // 3
    500000, // 4
    1000000 // 5
};

static uint64_t selectedAmnt;

static void setAmnt(void* arg) {
    selectedAmnt = *((uint64_t*)arg);
    SM_GOTO(txn == TXN_VOUCHER ? enterPass : enterPhone);
}

void setMtnChargeAmnt() {
    // 5-10-20-50-100
    ui_menu_addItem(&amntSelectionMenu, "50,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[1]);
    ui_menu_addItem(&amntSelectionMenu, "100,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[2]);
    ui_menu_addItem(&amntSelectionMenu, "200,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[3]);
    ui_menu_addItem(&amntSelectionMenu, "500,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[4]);
    ui_menu_addItem(&amntSelectionMenu, "1,000,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[5]);
}

void setMciChargeAmnt() {
    // 5-10-20-50
    ui_menu_addItem(&amntSelectionMenu, "50,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[1]);
    ui_menu_addItem(&amntSelectionMenu, "100,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[2]);
    ui_menu_addItem(&amntSelectionMenu, "200,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[3]);
    ui_menu_addItem(&amntSelectionMenu, "500,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[4]);
}

void setRightelChargeAmnt() {
    // 2-5-10-20-50
    ui_menu_addItem(&amntSelectionMenu, "20,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[0]);
    ui_menu_addItem(&amntSelectionMenu, "50,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[1]);
    ui_menu_addItem(&amntSelectionMenu, "100,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[2]);
    ui_menu_addItem(&amntSelectionMenu, "200,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[3]);
    ui_menu_addItem(&amntSelectionMenu, "500,000", LV_TEXT_ALIGN_LEFT, NULL,
                    setAmnt, &amnt[4]);
}

STATE_DEF_ENTER(SelectAmount) {
    ui_menu_create(&amntSelectionMenu, disp()->screen);
    LOG_TRACE("Operator = %d", selectedOp);
    switch (selectedOp) {
    case OPERATOR_MCI:
        setMciChargeAmnt();
        break;
    case OPERATOR_MTN:
        setMtnChargeAmnt();
        break;
    case OPERATOR_RIGHTEL:
        setRightelChargeAmnt();
        break;

    default:
        break;
    }
    GOTO_MENU(selectOperator, &amntSelectionMenu, NULL, NULL);
}

static void SelectAmount(State* parent) {
    selectAmount = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, selectAmount, parent, "select amount");
    selectAmount->vtable.enter = STATE_ENTER(SelectAmount);
}

/******************************************************************
 *                   Enter phone sub state
 ******************************************************************/

STATE_DEF_ENTER(EnterPhone) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_NUMBERS,
            .title  = phraseGetDef(PHRASE_PHONE_NUMBER),
            .info   = "",
            .maxLen = LEN_MAX_PHONE_NUMBER,
        },
        selectAmount, enterPass);
    inmgr()->set(INPUT_TYPE_KEYPAD, "09");
    inmgr()->setOut(phoneNum, phoneNum, sizeof(phoneNum));
}

static void EnterPhone(State* parent) {
    enterPhone = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPhone, parent, "enter phone");
    enterPhone->vtable.enter = STATE_ENTER(EnterPhone);
}

/******************************************************************
 *                   Enter pass sub state
 ******************************************************************/

STATE_DEF_ENTER(EnterPassword) {
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

static void EnterPassword(State* parent) {
    enterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, parent, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);
}

/******************************************************************
 *                   Communication sub state
 ******************************************************************/

STATE_DEF_ENTER(Communication) { txnStart(cfg, flow, state); }

static void Communication(State* parent) {
    communication = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, communication, parent, "Communication");
    communication->vtable.enter = STATE_ENTER(Communication);
}

static void createCommonStates(State* state) {
    CALL_ONCE(SelectOperator(state); SelectAmount(state); EnterPassword(state);
              Communication(state); EnterPhone(state););
}

/******************************************************************
 *                      Voucher Transaction
 ******************************************************************/

static int composeVoucher(TxnData* data) {
    composeCommon(data);
    data->core.amount         = selectedAmnt;
    data->extention.charge.op = selectedOp;
}

static const uint8_t isoFeildsVoucher[] = {ELEMENT_PAN,
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

const TxnFlowConfig voucherTxn = {

    .type = TXN_VOUCHER,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeildsVoucher,

    .feildsCnt = sizeof(isoFeildsVoucher),

    .compose = composeVoucher,

    .build = buildCommon,

    .parse = parseCommon,

    .done = financeTxnDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

STATE_DEF_ENTER(Voucher) {
    memset(flow, 0, sizeof(*flow));
    txn = TXN_VOUCHER;
    cfg = &voucherTxn;
    SM_GOTO(selectOperator);
}

OOP_CTOR(Voucher, State* parent, const char* name) {
    OOP_CALL_CTOR(Transaction, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Voucher);
    createCommonStates(&self->base.state);

    flow = self->base.flow;
}

/******************************************************************
 *                      TopUp Transaction
 ******************************************************************/

static int composeTopUp(TxnData* data) {
    composeCommon(data);
    data->core.amount         = selectedAmnt;
    data->extention.charge.op = selectedOp;
    memcpy(data->extention.charge.phoneNumber, phoneNum,
           sizeof(data->extention.charge.phoneNumber));
}

static const uint8_t isoFeildsTopUp[] = {ELEMENT_PAN,
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

    .type = TXN_TOPUP,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeildsTopUp,

    .feildsCnt = sizeof(isoFeildsTopUp),

    .compose = composeTopUp,

    .build = buildCommon,

    .parse = parseCommon,

    .done = financeTxnDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};

STATE_DEF_ENTER(TopUp) {
    memset(flow, 0, sizeof(*flow));
    txn = TXN_TOPUP;
    cfg = &topupTxn;
    SM_GOTO(selectOperator);
}

OOP_CTOR(TopUp, State* parent, const char* name) {
    OOP_CALL_CTOR(Transaction, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(TopUp);
    createCommonStates(&self->base.state);
    flow = self->base.flow;
}