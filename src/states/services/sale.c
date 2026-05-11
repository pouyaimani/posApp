#include "services.h"
#include "states/states.h"
#include "dev/dev.h"
#include "ui/ui.h"
#include "iso8583.h"
#include "receipt/receipt.h"
#include "settings/settings.h"
#include "txn.h"
#include "utility/utility.h"

static SubState *enterAmount;
static SubState *enterPass;
static SubState *commu;
static SubState *result;

STATE_DEF_ENTER(Sale) {
    SM_GOTO(enterAmount);
    // packer()->reset();
}

/******************** Enter amount sub state **********************/

static char *amount;

STATE_DEF_ENTER(EnterAmount) {
    GOTO_INPUT(STATE_IDLE, enterPass,
        "مبلغ", "", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
}

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {
    Input *in = getState(STATE_ID_INPUT);
    // OOP_CALL(packer(), setAmount, in->input);
    GOTO_INPUT(STATE_IDLE, commu,
        "رمز کارت", "", PASSWORD_MAX_LEN, IN_MODE_PASSWORD, NULL);
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Communication) {
    Input *in = getState(STATE_ID_INPUT);
    // OOP_CALL(packer(), setCardPwd, in->input);
    GOTO_ISO_TRANSMITTER(STATE_IDLE, result);
}

/*********************** Result sub state *************************/

STATE_DEF_ENTER(Result) {
    GOTO_TXN_RES(STATE_IDLE, STATE_IDLE);
}

/******************************************************************/

int8_t makeReceipt(TxnData *txn) {
    RETURN_VALUE_IF_NULL(txn, ;, ERR_NOK);
    Receipt rec;
    RETURN_VALUE_IF_NOT(createReceipt(&rec), true, 
        RECEIPT_CREATE_ERROR(), ERR_NOK);
    uint32_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    // Add header
    OOP_CALL(&rec, addHeader, date, time);
    DEFINE_STRING(terminal, 64);
    snprintf(terminal, sizeof(terminal), "%s:%s", settings()->terminal.terminalNo, "پایانه");
    DEFINE_STRING(code, 64);
    snprintf(code, sizeof(code), "%s:%s", settings()->terminal.terminalNo, "کد کارتخوان");
    RecColumn_t row1[] = {
        {terminal, LV_TEXT_ALIGN_LEFT, 1},
        {code, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(&rec, addText, 2, row1), ERR_OK, 
        OOP_CALL(&rec, destroy), ERR_NOK);

    // TODO: bank name
    RecColumn_t row2[] = {
        {"bank name", LV_TEXT_ALIGN_LEFT, 1},
        {"بانک", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(&rec, addText, 2, row2), ERR_OK, 
        OOP_CALL(&rec, destroy), ERR_NOK);
    
    DEFINE_STRING(trace, 64);
    snprintf(trace, sizeof(trace), "%s:%d", txn->core.trace, "پیگیری");
    DEFINE_STRING(ref, 64);
    snprintf(ref, sizeof(ref), "%s:%d", txn->core.refNum, "مرجع");
    RecColumn_t row3[] = {
        {ref, LV_TEXT_ALIGN_LEFT, 1},
        {trace, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(&rec, addText, 2, row3), ERR_OK, 
        OOP_CALL(&rec, destroy), ERR_NOK);

    OOP_CALL(&rec, addFooter);
    RETURN_VALUE_IF_NOT(OOP_CALL(&rec, addFooter), ERR_OK, 
        OOP_CALL(&rec, destroy), ERR_NOK);
    OOP_CALL(&rec, flush);
    OOP_CALL(&rec, destroy);
    return ERR_OK;

}

OOP_CTOR(Sale, State *parent, const char *name) {
    OOP_CALL_CTOR(Service, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Sale);
    self->base.vtable.makeReceipt = makeReceipt;

    enterAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, &self->base.state, "enter Amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterAmount);

    enterPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, &self->base.state, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    commu = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, commu, &self->base.state, "communication");
    commu->vtable.enter = STATE_ENTER(Communication);

    result = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, result, &self->base.state, "result");
    result->vtable.enter = STATE_ENTER(Result);
}