#include "services.h"
#include "states/states.h"
#include "dev/dev.h"
#include "ui/ui.h"
#include "iso8583.h"
#include "receipt/receiptTemplates.h"
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
    int8_t ret = buildReceipt(&rec, TXN_SALE, txn);
    RETURN_VALUE_IF_NOT(ret, ERR_OK, 
        {
            RECEIPT_CREATE_ERROR();
            OOP_CALL(&rec, destroy);
        },
        ret);
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