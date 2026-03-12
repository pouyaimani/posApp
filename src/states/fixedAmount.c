#include "states/states.h"
#include "display.h"
#include "dev/dev.h"
#include "ui/ui.h"
#include "storage/storage.h"
#include "states/merchant/merchant.h"
#include "utility/utility.h"

static int *fixedAmountItem;
static TerminalSettings *termStorage;

static SubState *amountList;
static SubState *singleAmount;
static SubState *variantAmount;

static Menu amountListMenu;

STATE_DEF_ENTER(AmountList) {
    uiMenu(&amountListMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < termStorage->amountListCnt ; i++) {
        char str[32] = {0};
        char amount[AMOUNT_MAX_CNT] = {0};
        amountSeparator(termStorage->amountList[i], amount, AMOUNT_MAX_CNT);
        snprintf(str, sizeof(str), "[ %s ] [ %s ]", amount, "ریال");
        OOP_CALL(&amountListMenu, addItem, str, NULL, NULL, NULL);
    }
    GOTO_MENU(getState(STATE_ID_IDLE), &amountListMenu, NULL);
}

STATE_DEF_ENTER(SingleAmount) {
    GOTO_INPUT(getState(STATE_ID_IDLE), getState(STATE_ID_IDLE), "", "", AMOUNT_MAX_CNT, IN_MODE_AMOUNT);
}

STATE_DEF_ENTER(VariantAmount) {
    char str[32] = {0};
    char amount[AMOUNT_MAX_CNT] = {0};
    amountSeparator(termStorage->amountList[11], amount, AMOUNT_MAX_CNT);
    snprintf(str, sizeof(str), "%s %s", "(ریال)", amount);
    GOTO_INPUT(getState(STATE_ID_IDLE), getState(STATE_ID_IDLE), str, "تعداد را وارد کنید", 4, IN_MODE_NUMBERS);
}

STATE_DEF_ENTER(FixedAmount) {
    if (*fixedAmountItem == FIXED_AMNT_LISTS) {
        SM_GOTO(amountList);
    } else if (*fixedAmountItem == FIXED_AMNT_SINGLE){
        SM_GOTO(singleAmount);
    } else if (*fixedAmountItem == FIXED_AMNT_VARIANT){
        SM_GOTO(variantAmount);
    }
}

OOP_CTOR(FixedAmount, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(FixedAmount);

    amountList = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, amountList, parent, "amount list");
    amountList->vtable.enter = STATE_ENTER(AmountList);

    singleAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, singleAmount, parent, "single amount");
    singleAmount->vtable.enter = STATE_ENTER(SingleAmount);

    variantAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, variantAmount, parent, "variant amount");
    variantAmount->vtable.enter = STATE_ENTER(VariantAmount);

    fixedAmountItem = &getStorage()->settings->terminal.fixedAmountItem;
    termStorage = &getStorage()->settings->terminal;
}