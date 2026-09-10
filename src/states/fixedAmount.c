#include "states/states.h"
#include "display.h"
#include "sys/sys.h"
#include "ui/menu.h"
#include "storage/storage.h"
#include "states/merchant/merchant.h"
#include "utility/utility.h"
#include "settings/settings.h"
#include "phrases/phrases.h"
#include "input/inputMgr.h"

static int*              fixedAmountItem;
static TerminalSettings* termStorage;

static SubState* amountList;
static SubState* singleAmount;
static SubState* variantAmount;

static Menu* amountListMenu;

STATE_DEF_ENTER(AmountList) {
    amountListMenu = MEM_ALLOC(sizeof(*amountListMenu));
    ui_menu_create(amountListMenu, disp()->screen);
    for (uint8_t i = 0; i < termStorage->amountListCnt; i++) {
        char str[32]                = {0};
        char amount[LEN_MAX_AMOUNT] = {0};
        amountSeparator(termStorage->amountList[i], amount, LEN_MAX_AMOUNT);
        snprintf(str, sizeof(str), "[ %s ] [ %s ]", amount, "ریال");
        ui_menu_addItem(amountListMenu, str, LV_TEXT_ALIGN_RIGHT, NULL, NULL,
                        NULL);
    }
    GOTO_MENU(STATE_IDLE, amountListMenu, NULL, NULL);
}

STATE_DEF_ENTER(SingleAmount) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_AMOUNT,
            .title  = "",
            .info   = "",
            .maxLen = LEN_MAX_AMOUNT,
        },
        STATE_IDLE, STATE_IDLE, NULL, NULL);
    // GOTO_INPUT(STATE_IDLE, STATE_IDLE, "", "", LEN_MAX_AMOUNT,
    // IN_MODE_AMOUNT,
    //            NULL);
}

STATE_DEF_ENTER(VariantAmount) {
    char str[32]                = {0};
    char amount[LEN_MAX_AMOUNT] = {0};
    amountSeparator(termStorage->amountList[11], amount, LEN_MAX_AMOUNT);
    snprintf(str, sizeof(str), "(%s) %s", phraseGetDef(PHRASE_RIAL), amount);
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_NUMBERS,
            .title  = str,
            .info   = phraseGetDef(PHRASE_ENTER_COUNT),
            .maxLen = 4,
        },
        STATE_IDLE, STATE_IDLE, NULL, NULL);
    // GOTO_INPUT(STATE_IDLE, STATE_IDLE, str, phraseGetDef(PHRASE_ENTER_COUNT),
    // 4,
    //            IN_MODE_NUMBERS, NULL);
}

STATE_DEF_ENTER(FixedAmount) {
    if (*fixedAmountItem == FIXED_AMNT_LISTS) {
        SM_GOTO(amountList);
    } else if (*fixedAmountItem == FIXED_AMNT_SINGLE) {
        SM_GOTO(singleAmount);
    } else if (*fixedAmountItem == FIXED_AMNT_VARIANT) {
        SM_GOTO(variantAmount);
    }
}

OOP_CTOR(FixedAmount, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(FixedAmount);

    amountList = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, amountList, parent, "amount list");
    amountList->vtable.enter = STATE_ENTER(AmountList);

    singleAmount = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, singleAmount, parent, "single amount");
    singleAmount->vtable.enter = STATE_ENTER(SingleAmount);

    variantAmount = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, variantAmount, parent, "variant amount");
    variantAmount->vtable.enter = STATE_ENTER(VariantAmount);

    fixedAmountItem = &settings()->terminal.fixedAmountItem;
    termStorage     = &settings()->terminal;
}