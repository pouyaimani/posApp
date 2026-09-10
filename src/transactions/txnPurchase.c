#include "transaction.h"
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
#include "ui/infoPage.h"
#include "txnCommon.h"

/******************************************************************
 *                           Substates
 ******************************************************************/
static SubState* enterAmount;
static SubState* enterPass;
static SubState* result;
static SubState* commu;

/******************************************************************
 *                Global variable within this file
 ******************************************************************/

static uint64_t amount;
static TxnFlow* flow;

extern const TxnFlowConfig purchaseTxn;

STATE_DEF_ENTER(Purchase) {
    memset(flow, 0, sizeof(*flow));
    SM_GOTO(enterAmount);
}

/******************************************************************
 *                Enter amount sub state
 ******************************************************************/

STATE_DEF_ENTER(EnterAmount) {
    inmgr()->run(
        &(InputCfg){
            .mode   = INMD_ENTER_AMOUNT,
            .title  = phraseGetDef(PHRASE_AMOUNT),
            .info   = phraseGetDef(PHRASE_TO_RIAL_1),
            .maxLen = LEN_MAX_AMOUNT,
        },
        STATE_IDLE, enterPass, NULL, NULL);
}

/******************************************************************
 *                Enter pass sub state
 ******************************************************************/

STATE_DEF_ENTER(EnterPassword) {
    amount = 0;
    if (strlen(inmgr()->input) < LEN_MIN_AMOUNT) {
        GOTO_INFO(enterAmount, enterAmount, INFO_ERROR,
                  phraseGetDef(PHRASE_AMOUNT_FLOOR_ER), "");
        return;
    }

    if (!str2u64(inmgr()->input, &amount)) {
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
        STATE_IDLE, commu, NULL, NULL);
}

/******************************************************************
 *                Communication sub state
 ******************************************************************/

STATE_DEF_ENTER(Communication) { txnStart(&purchaseTxn, flow, state); }

static int compose(TxnData* data) {
    composeCommon(data);
    data->core.amount = amount;
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

    TXN_FLOW_COMMON,

    .type = TXN_PURCHASE,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeilds,

    .needSettlement = true,

    .feildsCnt = sizeof(isoFeilds),

    .compose = compose,

    .done = financeTxnDone,
};

OOP_CTOR(Purchase, State* parent, const char* name) {
    OOP_CALL_CTOR(Transaction, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Purchase);

    enterAmount = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, &self->base.state, "enter Amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterAmount);

    enterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &self->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    commu = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, commu, &self->base.state, "communication");
    commu->vtable.enter = STATE_ENTER(Communication);

    flow = self->base.flow;
}